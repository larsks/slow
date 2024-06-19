#ifndef _BUFFER_H
#define _BUFFER_H

#include <time.h>

#ifndef BUFSIZE
#define BUFSIZE 1024
#endif

struct buffer {
  char data[BUFSIZE];
  int len;
  int pos;
  timer_t timer;
};

extern void buffer_init(struct buffer *);
extern void buffer_timer_start(struct buffer *, time_t, time_t);
extern int buffer_timer_expired(struct buffer *);
extern int buffer_empty(struct buffer *buf);
extern int buffer_write(struct buffer *buf, int fd, int len);

#endif // _BUFFER_H
