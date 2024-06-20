#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "must.h"

struct termios orig_attr;
static int orig_attr_valid = 0;

void restore_terminal() {
  MUST(tcsetattr(STDOUT_FILENO, TCSANOW, &orig_attr),
       "unable to restore terminal attributes");
}

void configure_terminal() {
  struct termios mod_attr;

  MUST(tcgetattr(STDOUT_FILENO, &orig_attr),
       "unable to get terminal attributes");
  orig_attr_valid = 1;
  memcpy(&mod_attr, &orig_attr, sizeof(struct termios));
  cfmakeraw(&mod_attr);
  MUST(tcsetattr(STDOUT_FILENO, TCSANOW, &mod_attr),
       "unable to set terminal attributes");

  atexit(restore_terminal);
}
