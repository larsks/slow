#include <signal.h>
#include <unistd.h>

#include "buffer.h"
#include "must.h"

void buffer_init(struct buffer *buf) {
  buf->len = 0;
  buf->pos = 0;
}

int buffer_write(struct buffer *buf, int fd, int len) {
  int nb;

  if (buf->pos + len > buf->len) {
    int oldlen = len;
    len = buf->len - buf->pos;
    fprintf(stderr, "warning: reducing write of %d bytes to %d bytes\n", oldlen,
            len);
  }
  MUST(nb = write(fd, &(buf->data[buf->pos++]), len), "write");

  if (buf->pos == buf->len) {
    buf->pos = 0;
    buf->len = 0;
    return -1;
  }

  return 0;
}

int buffer_empty(struct buffer *buf) { return buf->len == 0; }
