/* Opens a file up to the file limit and then closes them all. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void test_main(void) {
  int handle;

  for (int i = 0; i < 128; i++) {
    CHECK((handle = open("sample.txt")) > 1, "open \"sample.txt\"");
  }
  for (int j = 0; j < 128; j++) {
      msg("close \"sample.txt\"");
      close(handle);
  }
}
