#ifndef OCTET_H_2
#define OCTET_H_2

#include "frob3/frobtype.h"

// Object classes 1 through 15 do not contain pointers.
// Object classes 16 through 255 do contain pointers.
#define O_LAST_NONPTR_CLASS 15

// Public API.
void oinit(word begin, word end);
void ozero(byte* p, word len);
byte* oalloc_try(word len, byte cls);
byte* oalloc(word len, byte cls);
void ofree(byte* addr);
void omark(byte* addr);
void ogc();

// Global Variables.
extern word ORamBegin;
extern word ORamUsed;
extern word ORamEnd;
extern void (*OMarkRoots)(void);

#endif  // OCTET_H_2
