#define _GNU_SOURCE

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
#include "term.h"

#ifndef BUFSIZE
#define BUFSIZE 1024
#endif

#define OPTSTRING "b:hdit"
#define OPT_SPEED 'b'
#define OPT_HELP 'h'
#define OPT_NEED_STDIN 'i'
#define OPT_NEED_TTY 't'

#define DEFAULT_SPEED 9600
#define BITS_PER_BYTE 8
#define NANOSECONDS 1E9

struct {
  int speed;
  int need_stdin;
  int need_tty;
} options = {DEFAULT_SPEED, 0, 0};

char *progname;
struct timespec time_per_character;

void usage(FILE *out) {
  fprintf(out, "%s: usage: %s [-it] [-b bps] command [args [...]]\n", progname,
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
    default:
      usage(stderr);
      exit(2);
      break;
    }
  }

  return optind;
}

pid_t run_child(char **argv, int *child_stdin, int *child_stdout) {
  pid_t pid;
  int child_in_fds[2];
  int child_out_fds[2];

  if (options.need_tty) {
    struct winsize winp;

    MUST(ioctl(STDOUT_FILENO, TIOCGWINSZ, &winp),
         "unable to get terminal size");
    MUST(openpty(&child_out_fds[0], &child_out_fds[1], NULL, NULL, &winp),
         "openpty");
  } else {
    MUST(pipe(child_out_fds), "pipe");
  }

  if (options.need_stdin) {
    if (options.need_tty) {
      // if we have -it, then we want the same pty for stdin and stdout.
      // otherwise, running an interactive shell (e.g. bash) will have
      // problems with job control or signal handling.
      child_in_fds[0] = child_out_fds[1];
      child_in_fds[1] = child_out_fds[0];
    } else {
      MUST(pipe(child_in_fds), "pipe");
    }
  }

  MUST((pid = fork()), "fork");
  if (pid == 0) {
    // child

    if (options.need_tty) {
      setsid();

      MUST(ioctl(child_out_fds[1], TIOCSCTTY, 0),
           "failed to set controlling terminal");
    }

    if (options.need_stdin)
      MUST(dup2(child_in_fds[0], STDIN_FILENO), "dup2 (stdin)");
    else
      MUST(close(STDIN_FILENO), "close (stdin)");

    MUST(dup2(child_out_fds[1], STDOUT_FILENO), "dup2 (stdout)");
    MUST(dup2(child_out_fds[1], STDERR_FILENO), "dup2 (stderr)");

    for (int i = 0; i < 2; i++) {
      MUST(close(child_out_fds[i]), "close (out)");
      if (options.need_stdin && !options.need_tty)
        MUST(close(child_in_fds[i]), "close (in)");
    }

    MUST(execvp(argv[0], argv), "failed to run command");
  }

  // parent
  *child_stdout = child_out_fds[0];
  MUST(close(child_out_fds[1]), "close (child stdout)");

  if (options.need_stdin) {
    *child_stdin = child_in_fds[1];

    // When we're using a tty child_in_fds[0] == child_stdout, so
    // we don't want to close it.
    if (!options.need_tty)
      MUST(close(child_in_fds[0]), "close (child stdin)");
  } else {
    *child_stdin = -1;
  }

  return pid;
}

void loop(int child_stdin, int child_stdout) {
  struct timespec timeout;
  struct pollfd fds[2];
  struct buffer stdin_buf, stdout_buf;
  int ready = 0;
  int draining_stdin = !options.need_stdin;
  int draining_stdout = 0;
  int status;

  fds[0].fd = options.need_stdin ? STDIN_FILENO : -1;
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
            if (buffer_read(&stdin_buf, STDIN_FILENO, 0) == 0) {
              // stdin has closed
              fds[i].fd = -1;
              draining_stdin = 1;
            }
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

        if (fds[i].revents & POLLERR) {
          fprintf(stderr, "Error!\n");
          exit(1);
        }
      }
    }

    nanosleep(&time_per_character, NULL);
  }
}

int main(int argc, char *argv[]) {
  int optind;
  time_t t_start, t_stop;

  int child_stdin, child_stdout;
  pid_t pid;
  int status;

  optind = parse_args(argc, argv);
  if (argc - optind < 1) {
    usage(stderr);
    exit(2);
  }

  time_per_character.tv_sec = 0;
  time_per_character.tv_nsec = (NANOSECONDS / options.speed) * BITS_PER_BYTE;
  if (time_per_character.tv_nsec == 0) {
    // chartime is less than 1us
    fprintf(stderr, "WARNING: cannot limit to %d bps\n", options.speed);
  }

  if (options.need_tty) {
    configure_terminal();
  }

  pid = run_child(&argv[optind], &child_stdin, &child_stdout);

  signal(SIGPIPE, SIG_IGN);

  loop(child_stdin, child_stdout);

  MUST(waitpid(pid, &status, 0), "waiting for child to exit");
  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  else
    return 128 + WTERMSIG(status);
}
