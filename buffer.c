#include <signal.h>
#include <unistd.h>

#include "buffer.h"
#include "must.h"

void buffer_init(struct buffer *buf, size_t cap) {
  buf->data = (char *)malloc(cap);
  buf->capacity = cap;
  buf->len = 0;
  buf->pos = 0;
}

int buffer_read(struct buffer *buf, int fd, int len) {
  int available;
  int nb;

  available = buf->capacity - buf->len;
  if (available == 0)
    return 0;

  if (len == 0 || len > available)
    len = available;

  MUST((nb = read(fd, (buf->data + buf->len), len)), "read");
  buf->len += nb;
  return nb;
}

int buffer_write(struct buffer *buf, int fd, int len) {
  int nb;

  if (buf->pos + len > buf->len) {
    int oldlen = len;
    len = buf->len - buf->pos;
    fprintf(stderr, "warning: reducing write of %d bytes to %d bytes\n", oldlen,
            len);
  }
  MUST(nb = write(fd, (buf->data + buf->pos++), len), "write");

  if (buf->pos == buf->len) {
    buf->pos = 0;
    buf->len = 0;
    return -1;
  }

  return 0;
}

int buffer_empty(struct buffer *buf) { return buf->len == 0; }
size_t buffer_len(struct buffer *buf) { return buf->len; }
size_t buffer_cap(struct buffer *buf) { return buf->capacity; }
