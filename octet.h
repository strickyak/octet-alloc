#ifndef OCTET_H_
#define OCTET_H_

// Octet is composed of 8-bit unsigned bytes.
typedef unsigned char byte;
// Octet is addressed by 16-bit unsigned words.
typedef unsigned short word;
// Pointers to directly address Octet memory.
typedef char* optr;

#define O_mem_size 0x8000
#define O_start OctetMemory
#define O_limit (OctetMemory + O_mem_size)
#define O_LAST_NONPTR_CLASS 7 /* 1 thru 7 are not pointers */

// public api:

// memory access functions.
extern byte ogetb(word addr);
extern void oputb(word addr, byte x);
extern word ogetw(word addr);
extern void oputw(word addr, word x);
extern void ozero(word begin, word len);

// macro equivalents:
#define Ogetb(A) (ORam[(word)(A)])
#define Oputb(A, X) (ORam[(word)(A)] = (byte)(X), 0)
#define Ogetw(A) ((word)((((word)Ogetb(A)) << 8) | (word)Ogetb(A + 1)))
#define Oputw(A, X) (Oputb(A, (word)X >> 8), Oputb(A + 1, X), 0)

#define ogetb Ogetb
#define ogetw Ogetw
#define oputb Oputb
#define oputw Oputw

// direct pointers to memory.
extern optr oword2ptr(word addr);
extern word optr2word(optr ptr);

// Garbage collected alloc & gc.
extern void oinit();
extern word oalloc(word len, byte cls);
extern void ogc();

// Unsafe free.  Only use it if no other pointers exist.
extern void ofree(word addr);

// private:
extern byte ORam[O_mem_size];
#define O_num_buckets 13
extern byte osize2bucket(word size);
extern void opanic(byte);

// Error Numbers
#define OE_TOO_BIG 52    /* length of alloc cannot be over 256 */
#define OE_TOO_LATE 53   /* cannot call oallocforever() after oalloc() */
#define OE_ZERO_CLASS 54 /* class number can not be zero */
#define OE_OUT_OF_MEM 55 /* out of memory */

#endif  // OCTET_H_
