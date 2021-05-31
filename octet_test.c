#include "octet.h"

#include <assert.h>
#include <stdio.h>

void mark_no_roots() { printf(" ======GC====== "); }

void AllocAndPrintTest() {
  for (word j = 0; j < 10; j++) {
    printf("Round %d:\n", j);
    for (word i = 1; i < 256; i++) {
      word p = oalloc(i, i, mark_no_roots);
      printf("%d: $%04x ", i, p);
    }
    printf("\n");
  }
}

void opanic(byte x) {
  fflush(stdout);
  fprintf(stderr, "\n*** PANIC %d\n", x);
  assert(0);
}

int main() {
  printf("hello %d %d\n", sizeof(word), sizeof(short));
  printf("oinit:\n");
  oinit(200, 30000);
  printf("did oinit\n");
  AllocAndPrintTest();
  printf("bye\n");
#if 0
  oinit(), AllocAndPrintTest();
  oinit(), AllocAndPrintTest();
  oinit(), AllocAndPrintTest();
  oinit(), AllocAndPrintTest();
  oinit(), AllocAndPrintTest();
  oinit(), AllocAndPrintTest();
  oinit(), AllocAndPrintTest();
  oinit(), AllocAndPrintTest();
#endif
}
