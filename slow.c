#include <errno.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define OPTSTRING "b:hs"
#define OPT_SPEED 'b'
#define OPT_STATS 's'
#define OPT_HELP 'h'
#define DEFAULT_SPEED 9600
#define BUFSIZE 8192

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

  if (-1 == (tcgetattr(STDOUT_FILENO, &termp))) {
    perror("unable to get terminal attributes");
    exit(1);
  }
  if (-1 == (ioctl(STDOUT_FILENO, TIOCGWINSZ, &winp))) {
    perror("unable to get terminal size");
    exit(1);
  }
  if (-1 == (pid = forkpty(&tty, NULL, &termp, &winp))) {
    perror("fork");
    exit(1);
  }

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
  int chartime;
  int optind;
  time_t t_start, t_stop;
  int bytecount = 0;

  optind = parse_args(argc, argv);

  chartime = (1000000 / options.speed) * 8;
  if (chartime == 0) {
    // chartime is less than 1us
    fprintf(stderr, "WARNING: cannot limit to %d bps\n", options.speed);
  }

  tty = run_child_on_pty(&argv[optind]);

  // ignore SIGCHLD
  signal(SIGCHLD, SIG_IGN);

  t_start = time(NULL);
  while (!quit) {
    char buf[BUFSIZE];
    size_t nb;

    nb = read(tty, buf, sizeof(buf));
    switch (nb) {
    case -1:
      if (errno == EIO) {
        // we expect EIO if the child process exits
        quit = 1;
        continue;
      } else {
        perror("read");
        exit(1);
      }
      break;
    case 0:
      quit = 1;
      continue;
      break;
    default:
      bytecount += nb;

      for (int i = 0; i < nb; i++) {
        write(STDOUT_FILENO, &buf[i], 1);
        usleep(chartime);
      }
    }
  }
  t_stop = time(NULL);

  if (options.show_stats) {
    printf("bytes %d seconds %ld\n", bytecount, t_stop - t_start);
  }
}
