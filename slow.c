#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

#include "buffer.h"
#include "must.h"

#ifndef BUFSIZE
#define BUFSIZE 1024
#endif

#define OPTSTRING "b:hdit"
#define OPT_DEBUG 'd'
#define OPT_SPEED 'b'
#define OPT_HELP 'h'
#define OPT_NEED_STDIN 'i'
#define OPT_NEED_TTY 't'

#define DEFAULT_SPEED 9600
#define BITS_PER_BYTE 8
#define NANOSECONDS 1E9

struct {
  int speed;
  int wait_for_debugger;
  int need_stdin;
  int need_tty;
} options = {DEFAULT_SPEED, 0, 0, 0};

char *progname;
struct timespec time_per_character;
struct termios orig_attr;
int orig_attr_valid = 0;

void usage(FILE *out) {
  fprintf(out, "%s: usage: %s [-s] [-b bps] command [args [...]]\n", progname,
          progname);
}

int parse_args(int argc, char **argv) {
  int ch;

  progname = argv[0];

  while (-1 != (ch = getopt(argc, argv, OPTSTRING))) {
    switch (ch) {
    case OPT_NEED_STDIN:
      options.need_stdin = 1;
      break;
    case OPT_NEED_TTY:
      options.need_tty = 1;
      break;
    case OPT_SPEED:
      options.speed = atoi(optarg);
      if (options.speed == 0) {
        fprintf(stderr, "invalid speed: %s\n", optarg);
        exit(1);
      }
      break;
    case OPT_HELP:
      usage(stdout);
      exit(0);
      break;
    case OPT_DEBUG:
      options.wait_for_debugger = 1;
      break;
    default:
      usage(stderr);
      exit(2);
      break;
    }
  }

  return optind;
}

pid_t run_child_on_pty(char **argv, int *child_stdin, int *child_stdout) {
  struct winsize winp;
  pid_t pid;
  int child_in_fds[2];
  int child_out_fds[2];

  MUST(ioctl(STDOUT_FILENO, TIOCGWINSZ, &winp), "unable to get terminal size");

  if (options.need_tty) {
    // Note that we reverse the indexes in child_in_fds here so that the parent
    // ends up owning the master device.
    MUST(openpty(&child_in_fds[1], &child_in_fds[0], NULL, NULL, &winp),
         "openpty");
  } else {
    MUST(pipe(child_in_fds), "pipe");
  }

  MUST(openpty(&child_out_fds[0], &child_out_fds[1], NULL, NULL, &winp),
       "openpty");

  MUST((pid = fork()), "fork");
  if (pid == 0) {
    // child
    setsid();
    MUST(ioctl(child_out_fds[1], TIOCSCTTY, 0),
         "failed to set controlling terminal");
    MUST(dup2(child_in_fds[0], STDIN_FILENO), "dup2 (stdin)");
    MUST(dup2(child_out_fds[1], STDOUT_FILENO), "dup2 (stdout)");
    MUST(dup2(child_out_fds[1], STDERR_FILENO), "dup2 (stderr)");

    for (int i = 0; i < 2; i++) {
      MUST(close(child_in_fds[i]), "close (in)");
      MUST(close(child_out_fds[i]), "close (out)");
    }

    MUST(execvp(argv[0], argv), "failed to run command");
  }

  // parent
  *child_stdin = child_in_fds[1];
  *child_stdout = child_out_fds[0];

  MUST(close(child_in_fds[0]), "close");
  MUST(close(child_out_fds[1]), "close");

  return pid;
}

void loop(int child_stdin, int child_stdout, pid_t child_pid) {
  struct timespec timeout;
  struct pollfd fds[2];
  struct buffer stdin_buf, stdout_buf;
  int ready = 0;
  int draining_stdin = !options.need_stdin;
  int draining_stdout = 0;
  int status;

  fds[0].fd = STDIN_FILENO;
  fds[1].fd = child_stdout;
  fds[0].events = fds[1].events = POLLIN;

  timeout.tv_sec = 0;
  timeout.tv_nsec = 0;

  buffer_init(&stdin_buf, BUFSIZE);
  buffer_init(&stdout_buf, BUFSIZE);

  while (1) {
    if (!buffer_empty(&stdin_buf)) {
      buffer_write(&stdin_buf, child_stdin, 1);
    }
    if (!buffer_empty(&stdout_buf)) {
      buffer_write(&stdout_buf, STDOUT_FILENO, 1);
    }

    if (draining_stdout && buffer_empty(&stdout_buf)) {
      break;
    }
    if (draining_stdin && buffer_empty(&stdin_buf)) {
      close(child_stdin);
      draining_stdin = 0;
    }

    MUST((ready = ppoll(fds, 2, &timeout, NULL)), "ppoll");

    if (ready) {
      for (int i = 0; i < 2; i++) {
        if (fds[i].revents & POLLIN) {
          int nb;

          if (fds[i].fd == STDIN_FILENO) {
            buffer_read(&stdin_buf, STDIN_FILENO, 0);
          }

          if (fds[i].fd == child_stdout) {
            buffer_read(&stdout_buf, child_stdout, 0);
          }
        }

        if (fds[i].revents & POLLHUP) {
          if (fds[i].fd == STDIN_FILENO) {
            draining_stdin = 1;
          } else {
            draining_stdout = 1;
          }
          fds[i].fd = -1;
        }
      }
    }

    nanosleep(&time_per_character, NULL);
  }
}

void exit_and_restore(int status) {
  if (options.need_tty) {
    MUST(tcsetattr(STDOUT_FILENO, TCSANOW, &orig_attr),
         "unable to set terminal attributes");
  }

  exit(status);
}

int main(int argc, char *argv[]) {
  int optind;
  time_t t_start, t_stop;
  int bytecount = 0;

  int child_stdin, child_stdout;
  pid_t pid;

  optind = parse_args(argc, argv);

  time_per_character.tv_sec = 0;
  time_per_character.tv_nsec = (NANOSECONDS / options.speed) * BITS_PER_BYTE;
  if (time_per_character.tv_nsec == 0) {
    // chartime is less than 1us
    fprintf(stderr, "WARNING: cannot limit to %d bps\n", options.speed);
  }

  if (options.wait_for_debugger) {
    fprintf(stderr, "pid: %d\n", getpid());
    while (options.wait_for_debugger)
      sleep(1);
  }

  if (options.need_tty) {
    struct termios mod_attr;

    MUST(tcgetattr(STDOUT_FILENO, &orig_attr),
         "unable to get terminal attributes");
    orig_attr_valid = 1;
    memcpy(&mod_attr, &orig_attr, sizeof(struct termios));
    cfmakeraw(&mod_attr);
    MUST(tcsetattr(STDOUT_FILENO, TCSANOW, &mod_attr),
         "unable to set terminal attributes");
  }

  pid = run_child_on_pty(&argv[optind], &child_stdin, &child_stdout);

  signal(SIGCHLD, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);

  loop(child_stdin, child_stdout, pid);
  exit_and_restore(0);
}
