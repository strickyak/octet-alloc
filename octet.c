#include "octet.h"

// Emulated memory for testing on a large host.
// On a 64K platform, prefer raw memory somewhere.
byte ORam[O_mem_size];
word ORamUsed;
word ORamForever;

// Free chunks are link-listed from OBucket, by word addr.
// Word addr 0 means empty bucket or end of list.
word OBucket[O_num_buckets];
word OBucketCap[O_num_buckets] = {2,  4,  8,  12,  16,  24, 32,
                                  48, 64, 96, 128, 192, 256};

byte osize2bucket(word size) {
  for (byte buck = 0; buck < O_num_buckets; buck++) {
    if (size <= OBucketCap[buck]) return buck;
  }
  opanic(OE_TOO_BIG);  // Request was too big.
  return 0;            // Not reached.
}

void oinit() {
  for (byte i = 0; i < O_num_buckets; i++) OBucket[i] = 0;
  for (word a = 0; a < O_mem_size; a++) ORam[a] = 0;
  ORamUsed = 2;     // Don't use 0.  Keep pointers even.
  ORamForever = 2;  // Don't use 0.  Keep pointers even.
}

void ozero(word begin, word len) {
  for (word i = 0; i < len; i++) oputb(begin + i, 0);
}

word oallocforever(word len, byte cls) {
  if (len & 1) ++len;
  if (len > 256) opanic(OE_TOO_BIG);
  if (ORamUsed != ORamForever) opanic(OE_TOO_LATE);

  word p = ORamUsed;
  oputb(p, len >> 1);
  oputb(p + 1, cls);
  ozero(p + 2, len);

  ORamUsed += len + 2;
  ORamForever = ORamUsed;
  return p + 2;
}

word oalloc_try(word len, byte cls) {
  if (!cls) opanic(OE_ZERO_CLASS);
  byte buck = osize2bucket(len);
  word p = OBucket[buck];
  if (p) {
    OBucket[buck] = ogetw(p);
    word plen = 2 + ((word)ogetb(p - 2) << 1);
    ozero(p, plen);
    oputb(p - 1, cls);
    return p;
  }

  // Carve a new one.
  word cap = OBucketCap[buck];
  word sz = cap + 2;
  p = ORamUsed;
  if (p + sz >= O_mem_size - 1) return 0;
  oputb(p, cap >> 1);
  oputb(p + 1, cls);
  ozero(p + 2, cap);

  ORamUsed += sz;
  return p + 2;
}

word oalloc(word len, byte cls) {
  // First try.
  word p = oalloc_try(len, cls);
  if (!p) {
    // Garbage collect, and try again.
    ogc();
    // Second try.
    p = oalloc_try(len, cls);
  }
  if (!p) opanic(OE_OUT_OF_MEM);
  return p;
}

void ofree(word addr) {
  word len = 2 + ((word)ogetb(addr - 2) << 1);
  ozero(addr - 1, len + 1);  // Clear the class and the data.
  byte buck = osize2bucket(len);
  oputw(addr, OBucket[buck]);
  OBucket[buck] = addr;
}

void ogc() {
  // No marks are set.
  // TODO -- what are our roots?
  // word p = ORamForever;
}
