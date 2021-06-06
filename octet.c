#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#include "octet.h"

// Emulated memory for testing on a large host.
// On a 64K platform, prefer raw memory somewhere.
byte ORam[1 << 16];
word ORamUsed;
word ORamForever;
word ORamBegin;
word ORamEnd;
omarker OMarkerFn;

// Free chunks are link-listed from OBucket, by word addr.
// Word addr 0 means empty bucket or end of list.
word OBucket[O_NUM_BUCKETS];
byte OBucketCap[] = {2, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 254};

bool ovalidaddr(word a) {
  return ((a & 1) == 0 && ORamBegin < a && a < ORamEnd);
}

byte ocap(word a) {
  word c = 0x7F & ogetb(a - DCAP);
  return c + c;
}
byte ocls(word a) { return ogetb(a - DCLS); }

byte osize2bucket(byte size) {
  for (byte buck = 0; buck < O_NUM_BUCKETS; buck++) {
    if (size <= OBucketCap[buck]) {
      return buck;
    }
  }
  opanic(OE_TOO_BIG);  // Request was too big.
  return 0;            // Not reached.
}

void oinit(word begin, word end, omarker fn) {
  assert2((int)sizeof(OBucketCap) / (int)sizeof(byte) == O_NUM_BUCKETS,
          "%d should == %d", (int)sizeof(OBucketCap) / (int)sizeof(byte),
          O_NUM_BUCKETS);
  ORamBegin = begin;
  ORamEnd = end;
  OMarkerFn = fn;
  ORamUsed = begin;
  ORamForever = begin;
  for (byte i = 0; i < O_NUM_BUCKETS; i++) OBucket[i] = 0;
  ozero(ORamBegin, ORamEnd);
#if 0
#if GUARD
  oputb(ORamBegin, GUARD_ONE);
#endif
#endif
}

void ozero(word begin, word len) {
  for (word i = 0; i < len; i++) oputb(begin + i, 0);
}

void oassertzero(word begin, word len) {
  for (word i = 0; i < len; i++) {
    assert2(!ogetb(begin + i), "nonzero=%d at %d", (int)ogetb(begin + i),
            (int)i);
  }
}

word ocarve(byte len, byte cls) {
  if (!cls) opanic(OE_ZERO_CLASS);
  if (len == 0xFF) opanic(OE_TOO_BIG);
  if (len & 1) ++len;
  byte buck = osize2bucket(len);
  byte cap = OBucketCap[buck];

  // reserve both initial and spare final header.
  word delta = DHDR + cap;
  word final = ORamUsed + delta;
  word final_plus_guard = final + GUARD;
  // detect out of memory.
  if (final_plus_guard >= ORamEnd) return 0;
  // detect overflow (if ORamEnd is very large).
  if (final_plus_guard < ORamUsed) return 0;

  word p = ORamUsed + DHDR;
#if GUARD
  oputb(p - 4, GUARD_ONE);
  oputb(p - 1, GUARD_TWO);
  oputb(final, GUARD_ONE);
#endif
  // TODO: use smaller amount if Forever.
  oputb(p - DCAP, cap >> 1);
  assert(ocap(p) >= len);
  oputb(p - DCLS, cls);
  oassertzero(p, cap);

  ORamUsed = final;
  return p;
}

word oallocforever(byte len, byte cls) {
  if (ORamForever != ORamUsed) opanic(OE_TOO_LATE);
  word p = ocarve(len, cls);
  if (!p) opanic(OE_OUT_OF_MEM);
  ORamForever = ORamUsed;
  return p;
}
void ocheckguards(word p) {
#if GUARD
  assert(ogetb(p - 4) == GUARD_ONE);
  assert(ogetb(p - 1) == GUARD_TWO);
  word final = p + ocap(p);
  assert(ogetb(final) == GUARD_ONE);
#endif
}
word oalloc_try(byte len, byte cls) {
  if (!cls) opanic(OE_ZERO_CLASS);
  byte buck = osize2bucket(len);
  // Get p from the head of the linked list.
  word p = OBucket[buck];
  if (p) {
#if GUARD
    ocheckguards(p);
#endif
    // Remove p from the head of the linked list.
    OBucket[buck] = ogetw(p);  // next link is at p.
    oputw(p, 0);               // Clear where the link was.
    oputb(p - DCLS, cls);
    word cap = ocap(p);
    assert(cap >= len);
    oassertzero(p, cap);
    printf("reuse: buck=%d cap=%d p=%d cls=%d\n", buck, cap, p, cls);
    return p;
  }

  // Carve a new one.
  return ocarve(len, cls);
}

word oalloc(byte len, byte cls) {
  // First try.
  word p = oalloc_try(len, cls);
  if (!p) {
    // Garbage collect, and try again.
    ogc();
    // Second try.
    p = oalloc_try(len, cls);
  }
  printf("oalloc %d %d -> %d\n", len, cls, p);
  if (!p) opanic(OE_OUT_OF_MEM);
  return p;
}

void ofree(word addr) {
#if GUARD
  ocheckguards(addr);
#endif
  oputb(addr - DCLS, 0);  // Clear the class.
  word cap = ocap(addr);
  ozero(addr, cap);  // Clear the data.
  byte buck = osize2bucket(cap);
  oputw(addr, OBucket[buck]);
  OBucket[buck] = addr;
}

void omark(word addr) {
#if GUARD
  ocheckguards(addr);
#endif
  if (!addr) opanic(OE_NULL_PTR);
  if (addr & 1) opanic(OE_BAD_PTR);
  if (addr < ORamBegin) opanic(OE_BAD_PTR);
  if (addr >= ORamEnd) opanic(OE_BAD_PTR);
  byte cls = ogetb(addr - 1);
  if (!cls) opanic(OE_ZERO_CLASS);

  oputb(addr - 2, 0x80 | ogetb(addr - 2));

  if (cls > O_LAST_NONPTR_CLASS) {
    // The payload is pointers.
    word len = ocap(addr);
    for (word i2 = 0; i2 < len; i2 += 2) {
      word q = ogetw(addr + i2);
      if (ovalidaddr(q)) {
        // Looks like q is an object pointer.
        // See if it is marked yet.
        byte qmark = 0x80 & ogetb(q - 2);
        // If not, recurse to mark and visit the object.
        if (!qmark) omark(q);
      }
    }
  }
}

void ogc() {
  printf("ogc: begin {{{\n");
  // Mark all our roots.
  OMarkerFn();

  // TODO: mark from permanent objs.
#if 0
  {word p = ORamBegin;
    while (p < oRamForever) {
	    ......
    }
  }
#endif

  // Reset all the buckets.
  for (byte i = 0; i < O_NUM_BUCKETS; i++) OBucket[i] = 0;

  printf("ogc: sweep ===\n");
  word p = ORamForever + DHDR;
  while (p < ORamUsed) {
#if GUARD
    ocheckguards(p);
#endif
    byte cls = ocls(p);
    word cap = ocap(p);
    byte mark_bit = 0x80 & ogetb(p - DCAP);
    // If it's unused (its class is 0 or mark bit is not set):
    // TODO: !cls doesn't mean anything.
    if (!cls || !mark_bit) {
      oputb(p - DCLS, 0);  // Clear class.
      ozero(p, cap);       // Clear payload.
      byte buck = osize2bucket(cap);
      // Add p to front of linked list.
      oputw(p, OBucket[buck]);
      OBucket[buck] = p;
    } else {
      oputb(p - DCAP, ogetb(p - DCAP) & 0x7F);  // clear the mark bit.
    }
    p += cap + DHDR;
  }
  printf("ogc: end }}}\n");
}

char* oshow(word addr) {
  word cap = ocap(addr);
  char* s = malloc(cap * 3 + 32);
  sprintf(s, "cap=%d[%02x %02x]:", cap, ogetb(addr - DCAP), ogetb(addr - DCLS));
  int n = strlen(s);
  for (int i = 0; i < cap; i += 2) {
    sprintf(s + n + 2 * i + i / 2, " %02x%02x", ogetb(addr + i),
            ogetb(addr + i + 1));
  }
  return s;
}

char* osay(word addr, const char* label, int index) {
  char* s = oshow(addr);
  printf("[%s #%d] %04x %s\n", label, index, addr, s);
  free(s);
#if GUARD
  ocheckguards(addr);
#endif
}

void odump() {
  //
}
