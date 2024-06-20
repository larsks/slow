#ifndef _MUST_H
#define _MUST_H

#include <stdio.h>
#include <stdlib.h>

extern void exit_and_restore(int);

#define MUST(x, msg)                                                           \
  {                                                                            \
    if (-1 == (x)) {                                                           \
      perror(msg);                                                             \
      exit_and_restore(1);                                                     \
    }                                                                          \
  }

#endif // _MUST_H
