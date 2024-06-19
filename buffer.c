#include <signal.h>
#include <unistd.h>

#include "buffer.h"
#include "must.h"

void buffer_init(struct buffer *buf) {
  struct sigevent sevp;
  buf->len = 0;
  buf->pos = 0;

  sevp.sigev_notify = SIGEV_NONE;
  MUST(timer_create(CLOCK_MONOTONIC, &sevp, &(buf->timer)), "timer_create");
}

int buffer_timer_expired(struct buffer *buf) {
  struct itimerspec tval;
  MUST(timer_gettime(buf->timer, &tval), "timer_gettime");
  return tval.it_value.tv_sec == 0 && tval.it_value.tv_nsec == 0;
}

void buffer_timer_start(struct buffer *buf, time_t sec, time_t nsec) {
  struct itimerspec tval;
  tval.it_interval.tv_sec = 0;
  tval.it_interval.tv_nsec = 0;
  tval.it_value.tv_sec = sec;
  tval.it_value.tv_nsec = nsec;
  MUST(timer_settime(buf->timer, 0, &tval, NULL), "timer_settime");
}

int buffer_write(struct buffer *buf, int fd, int len) {
  int nb;

  if (buf->pos + len > buf->len) {
    fprintf(stderr, "write too large\n");
    exit(1);
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
