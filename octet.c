#include "octet.h"

// Emulated memory for testing on a large host.
// On a 64K platform, prefer raw memory somewhere.
byte ORam[1 << 16];
word ORamUsed;
word ORamForever;
word ORamEnd;

// Free chunks are link-listed from OBucket, by word addr.
// Word addr 0 means empty bucket or end of list.
word OBucket[O_num_buckets];
word OBucketCap[O_num_buckets] = {4,  8,  12, 16,  24,  32,
                                  48, 64, 96, 128, 192, 256};

byte osize2bucket(word size) {
  for (byte buck = 0; buck < O_num_buckets; buck++) {
    if (size <= OBucketCap[buck]) return buck;
  }
  opanic(OE_TOO_BIG);  // Request was too big.
  return 0;            // Not reached.
}

void oinit(word begin, word end) {
  ORamUsed = begin;     // Don't use 0.  Keep pointers even.
  ORamForever = begin;  // Don't use 0.  Keep pointers even.
  ORamEnd = end;
  for (byte i = 0; i < O_num_buckets; i++) OBucket[i] = 0;
  for (word a = 0; a < ORamEnd; a++) ORam[a] = 0;
}

void ozero(word begin, word len) {
  for (word i = 0; i < len; i++) oputb(begin + i, 0);
}

word oallocforever(word len, byte cls) {
  if (len & 1) ++len;
  if (len > 256) opanic(OE_TOO_BIG);
  if (ORamUsed != ORamForever) opanic(OE_TOO_LATE);

  word p = ORamUsed;
  oputb(p, (len - 4) >> 1);
  oputb(p + 1, cls);
  ozero(p + 2, len);

  ORamUsed += len + 2;
  ORamForever = ORamUsed;
  return p + 2;
}

word oalloc_try(word len, byte cls) {
  if (!cls) opanic(OE_ZERO_CLASS);
  byte buck = osize2bucket(len);
  // Get p from the head of the linked list.
  word p = OBucket[buck];
  if (p) {
    // Remove p from the head of the linked list.
    OBucket[buck] = ogetw(p);
    word plen = 4 + ((word)ogetb(p - 2) << 1);
    ozero(p, plen);
    oputb(p - 1, cls);
    return p;
  }

  // Carve a new one.
  word cap = OBucketCap[buck];
  word sz = cap + 2;
  p = ORamUsed;
  if (p + sz >= ORamEnd - 1) return 0;
  oputb(p, (cap - 4) >> 1);
  oputb(p + 1, cls);
  ozero(p + 2, cap);

  ORamUsed += sz;
  return p + 2;
}

word oalloc(word len, byte cls, omarker fn) {
  // First try.
  word p = oalloc_try(len, cls);
  if (!p) {
    // Garbage collect, and try again.
    ogc(fn);
    // Second try.
    p = oalloc_try(len, cls);
  }
  if (!p) opanic(OE_OUT_OF_MEM);
  return p;
}

void ofree(word addr) {
  word len = 4 + ((word)ogetb(addr - 2) << 1);
  ozero(addr - 1, len + 1);  // Clear the class and the data.
  byte buck = osize2bucket(len);
  oputw(addr, OBucket[buck]);
  OBucket[buck] = addr;
}

void omark(word addr) {
  if (!addr) opanic(OE_NULL_PTR);
  if (addr & 1) opanic(OE_BAD_PTR);
  if (addr >= ORamEnd) opanic(OE_BAD_PTR);
  byte cls = ogetb(addr - 1);
  if (!cls) opanic(OE_ZERO_CLASS);

  oputb(addr - 2, 0x80 | ogetb(addr - 2));

  if (cls > O_LAST_NONPTR_CLASS) {
    // The payload is pointers.
    word len = 4 + ((word)ogetb(addr - 2) << 1);
    for (word i = 0; i < len; i += 2) {
      word q = ogetw(addr + i);
      if (q && (q & 1) == 0 && q < ORamEnd) {
        // Looks like q is an object pointer.
        // See if it is marked yet.
        byte qmark = 0x80 & ogetb(q - 2);
        // If not, recurse to mark and visit the object.
        if (!qmark) omark(q);
      }
    }
  }
}

void ogc(omarker fn) {
  // Mark all our roots.
  fn();

  // Reset all the buckets.
  for (byte i = 0; i < O_num_buckets; i++) OBucket[i] = 0;

  word p = ORamForever + 2;
  while (p < ORamEnd) {
    byte cls = ogetb(p - 1);
    byte hdr = ogetb(p - 2);
    word len = 4 + ((word)hdr << 1);
    // If it's unused (its class is 0 or mark bit is not set):
    if (!cls || (0x80 & hdr) == 0) {
      ozero(p - 1, len + 1);  // Clear class and payload.
      byte buck = osize2bucket(len);
      // Add p to front of linked list.
      oputw(p, OBucket[buck]);
      OBucket[buck] = p;
    }
    p += len + 2;
  }
}
