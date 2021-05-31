#include "octet.h"

#include <assert.h>
#include <stdio.h>

void AllocAndPrintTest() {
  for (word i = 1; i < 256; i++) {
    word p = oalloc(i, i);
    printf("%d: $%04x ", i, p);
  }
  printf("\n");
}

void opanic(byte x) {
  fflush(stdout);
  fprintf(stderr, "\n*** PANIC %d\n", x);
  assert(0);
}

int main() {
  printf("hello %d %d\n", sizeof(word), sizeof(short));
  oinit();
  printf("middle\n");
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
