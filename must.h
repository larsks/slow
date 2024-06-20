#ifndef _MUST_H
#define _MUST_H

#include <stdio.h>
#include <stdlib.h>

#define MUST(x, msg)                                                           \
  {                                                                            \
    if (-1 == (x)) {                                                           \
      perror(msg);                                                             \
      exit(1);                                                                 \
    }                                                                          \
  }

#endif // _MUST_H
