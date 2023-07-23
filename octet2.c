#include "octet2.h"

typedef struct hdr {
  byte buck;  // High bit is for mark.
#define BUCK_MARK_BIT 0x80
#define BUCK_MASK 0x7F
  byte cls;   // 0 is for free.
  // The actual header ends here.  It has size 2.
#define HDR_SIZE 2 /* not sizeof(Hdr)! */

  // If object is on the free list, this field `next`
  // is in the body of the object.
  struct hdr* next;
} Hdr;

#define OBJ_TO_HDR(P) ((Hdr*)((word)(P)-HDR_SIZE))
#define HDR_TO_OBJ(H) ((byte*)((word)(H)+HDR_SIZE))

#define NUM_BUCKETS 16
extern void opanic(const char*);

// macro equivalents:
#define getb(A)     (*(byte*)(A))
#define putb(A, X)  (*(byte*)(A) = (byte)(X))
#define getw(A)     (*(word*)(A))
#define putw(A, X)  (*(word*)(A) = (word)(X))

word ORamBegin;
word ORamUsed;
word ORamEnd;
void (*OMarkRoots)(void);

// Free chunks are link-listed from OBucket, by word addr.
// Word addr 0 means empty bucket or end of list.
Hdr* OBucket[NUM_BUCKETS];
word OBucketCap[NUM_BUCKETS] = {
    4,  8,  12, 16,  24,  32,
    48, 64, 96, 128, 192, 0x100,
    0x180, 0x200, 0x300, 0x400};

byte osize2bucket(word size) {
  for (byte buck = 0; buck < NUM_BUCKETS; buck++) {
    if (size <= OBucketCap[buck]) return buck;
  }
  opanic("TOOBIG");  // Request was too big.
  return 0;            // Not reached.
}

void oinit(word begin, word end) {
  ORamBegin = begin;     // Don't use 0.  Keep pointers even.
  ORamUsed = begin;     // Don't use 0.  Keep pointers even.
  ORamEnd = end;
}

void ozero(byte* p, word len) {
  word nw = len>>1;
  for (word i = 0; i < nw; i++) {
    putw(p, 0);
    p += sizeof(word);
  }
  if (len&1) putb(p, 0);
}

byte* oalloc_try(word len, byte cls) {
  if (!cls) opanic("0CLS");
  byte buck = osize2bucket(len);
  // Get p from the head of the linked list.
  Hdr* hdr = OBucket[buck];
  word cap = OBucketCap[buck];
  if (hdr) {
    // Remove hdr from the head of the linked list.
    OBucket[buck] = hdr->next;
    ozero((byte*)hdr, cap);
    hdr->cls = cls;
    return HDR_TO_OBJ(hdr);
  }

  // Carve a new one.
  word sz = cap + HDR_SIZE;
  if (ORamUsed + sz >= ORamEnd) return NULL;

  hdr = (Hdr*)ORamUsed;
  ORamUsed += sz;
  hdr->buck = buck;
  hdr->cls = cls;
  byte* obj = HDR_TO_OBJ(hdr);
  ozero(obj, cap);

  return obj;
}

byte* oalloc(word len, byte cls) {
  // First try.
  byte* p = oalloc_try(len, cls);
  if (!p) {
    // Garbage collect, and try again.
    ogc();
    // Second try.
    p = oalloc_try(len, cls);
  }
  // TODO: sbrk more ram.
  if (!p) opanic("OOM");
  return p;
}

byte* orealloc(byte* obj, word len, bool to_free) {
  Hdr* hdr = OBJ_TO_HDR(obj);
  byte buck = hdr->buck;
  word cap = OBucketCap[buck];
  if (cap < len) {
    byte* p = oalloc(len, hdr->cls);
    for (word i = 0; i < len; i++) p[i] = obj[i]; // copy data over.
    if (to_free) ofree(obj);
    return p;
  }
  return obj;
}

void ofree(byte* addr) {
  Hdr* h = OBJ_TO_HDR(addr);
  byte buck = h->buck;
  word cap = OBucketCap[buck];

  ozero(addr, cap);  // clear the object.
  h->cls = 0;
  h->next = OBucket[buck];
  OBucket[buck] = h;
}

void omark(byte* addr) {
  if (!addr) return;

  if (addr & 1) opanic("ODD");
  if ((word)addr < ORamBegin) opanic("BADPTR");
  if ((word)addr >= ORamEnd) opanic("BADPTR");
  Hdr* hdr = OBJ_TO_HDR(addr);
  byte cls = hdr->cls;
  if (!cls) opanic("0CLS");
  hdr->buck = BUCK_MARK_BIT | hdr->buck;

  if (cls > O_LAST_NONPTR_CLASS) {
    // The payload is pointers.
    word cap = OBucketCap[hdr->buck];
    for (word i = 0; i < cap; i += 2) {
      word q = getw(addr + i);
      if (q > ORamBegin && q < ORamEnd && (q & 1) == 0) {
        // Looks like q is an object pointer.
        // See if it is marked yet.
        byte marked = OBJ_TO_HDR(q)->buck & BUCK_MARK_BIT;
        // If not, recurse to mark and visit the object.
        if (!marked) omark((byte*)q);
      }
    }
  }
}

void ogc() {
  // If OMarkRoots is not set, then there is no garbage collection:
  // we are just using the library like malloc() and free().
  if (!OMarkRoots) return;

  // Mark all our roots, and all reachable from the roots.
  OMarkRoots();
  
  // Reset all the free-list buckets.
  for (byte i = 0; i < NUM_BUCKETS; i++) OBucket[i] = 0;

  Hdr* p = (Hdr*)ORamBegin;
  while ((word)p < ORamUsed) {
    byte cls = p->cls;
    byte buck = BUCK_MASK & p->buck;
    word cap = OBucketCap[buck];

    // If it's unused (its class is 0 or mark bit is not set):
    if (!cls || !(BUCK_MARK_BIT & buck)) {
      ozero(HDR_TO_OBJ(p), cap);  // Clear the object.
      p->cls = 0;  // Class 0 means unused.
      // Add p to front of linked list.
      p->next = OBucket[buck];
      OBucket[buck] = p;
    }
    p = (Hdr*) ((word)p + HDR_SIZE + cap);
  }
}
