#ifndef _MUST_H
#define _MUST_H

#include <stdio.h>
#include <stdlib.h>

// If err != 0, print an error message (including the text of msg) and exit.
// The error message will include the filename, line number, and the name of
// the enclosing function.
#define MUST(err, msg) _must(__FILE__, __LINE__, __func__, #err, (err), msg)

// This is the support function for the MUST macro.
void _must(const char *fileName, int lineNumber, const char *funcName,
           const char *calledFunction, int err, char *msg);

#endif // _MUST_H
