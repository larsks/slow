#ifndef _BUFFER_H
#define _BUFFER_H

#include <time.h>

struct buffer {
  char *data;
  int capacity;
  int len;
  int pos;
};

extern void buffer_init(struct buffer *, size_t);
extern int buffer_read(struct buffer *buf, int fd, int len);
extern int buffer_write(struct buffer *buf, int fd, int len);
extern int buffer_empty(struct buffer *buf);
extern size_t buffer_len(struct buffer *buf);
extern size_t buffer_cap(struct buffer *buf);

#endif // _BUFFER_H
