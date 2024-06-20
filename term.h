#ifndef _TERM_H
#define _TERM_H

#include <termios.h>

extern struct termios orig_attr;

void configure_terminal(void);
void restore_terminal(void);

#endif // _TERM_H
