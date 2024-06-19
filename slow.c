#define _GNU_SOURCE

#include <errno.h>
#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "buffer.h"
#include "must.h"

#define OPTSTRING "b:hs"
#define OPT_SPEED 'b'
#define OPT_STATS 's'
#define OPT_HELP 'h'

#define DEFAULT_SPEED 9600
#define BITS_PER_BYTE 8
#define NANOSECONDS 1E9

struct {
  int speed;
  int show_stats;
} options = {DEFAULT_SPEED, 0};

char *progname;

void usage(FILE *out) {
  fprintf(out, "%s: usage: %s [-s] [-b bps] command [args [...]]\n", progname,
          progname);
}

int parse_args(int argc, char **argv) {
  int ch;

  progname = argv[0];

  while (-1 != (ch = getopt(argc, argv, OPTSTRING))) {
    switch (ch) {
    case OPT_SPEED:
      options.speed = atoi(optarg);
      if (options.speed == 0) {
        fprintf(stderr, "invalid speed: %s\n", optarg);
        exit(1);
      }
      break;
    case OPT_STATS:
      options.show_stats = 1;
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

int run_child_on_pty(char **argv) {
  struct termios termp;
  struct winsize winp;
  pid_t pid;
  int tty;

  MUST(tcgetattr(STDOUT_FILENO, &termp), "unable to get terminal attributes");

  cfmakeraw(&termp);

  MUST(ioctl(STDOUT_FILENO, TIOCGWINSZ, &winp), "unable to get terminal size");
  MUST(pid = forkpty(&tty, NULL, &termp, &winp), "forkpty");

  if (pid == 0) {
    // child
    execvp(argv[0], argv);

    // if exec failed...
    perror("failed to run command");
    exit(1);
  }

  // parent
  return tty;
}

int main(int argc, char *argv[]) {
  int tty;
  int quit = 0;
  int optind;
  time_t t_start, t_stop;
  struct timespec time_per_character;
  int bytecount = 0;

  int nfds = 2;
  struct pollfd fds[2];
  struct buffer bufs[2];
  struct timespec timeout;

  optind = parse_args(argc, argv);

  time_per_character.tv_sec = 0;
  time_per_character.tv_nsec = (NANOSECONDS / options.speed) * BITS_PER_BYTE;
  if (time_per_character.tv_nsec == 0) {
    // chartime is less than 1us
    fprintf(stderr, "WARNING: cannot limit to %d bps\n", options.speed);
  }

  tty = run_child_on_pty(&argv[optind]);

  // ignore SIGCHLD
  signal(SIGCHLD, SIG_IGN);

  fds[0].fd = STDIN_FILENO;
  fds[1].fd = tty;
  fds[0].events = fds[1].events = POLLIN;

  for (int i = 0; i < 2; i++)
    buffer_init(&bufs[i]);

  timeout.tv_sec = 0;
  timeout.tv_nsec = 0;

  t_start = time(NULL);
  while (1) {
    int ready;

    MUST((ready = ppoll(fds, nfds, &timeout, NULL)), "ppoll");

    if (ready) {
      for (int i = 0; i < nfds; i++) {
        if (fds[i].revents & POLLIN) {
          int bytesread;

          MUST(
              (bytesread = read(fds[i].fd, bufs[i].data, sizeof(bufs[i].data))),
              "read");

          bufs[i].len = bytesread;
        }
        if (fds[i].revents & POLLHUP) {
          close(fds[i].fd);
          fds[i].fd = -1;
          quit = 1;
        }
      }
    }

    // we have data from stdin to write to tty
    if (!buffer_empty(&bufs[0])) {
      buffer_write(&bufs[0], tty, 1);
    }

    // we have data from tty to write to stdout
    if (!buffer_empty(&bufs[1])) {
      buffer_write(&bufs[1], STDOUT_FILENO, 1);
    }

    nanosleep(&time_per_character, NULL);
  }
  t_stop = time(NULL);

  if (options.show_stats) {
    printf("bytes %d seconds %ld\n", bytecount, t_stop - t_start);
  }
}
