/* Child process run by exec-multiple, exec-one, wait-simple, and
   wait-twice tests.
   Just prints a single message and terminates. */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include "tests/lib.h"


const char* test_name = "custom-child";

int main(int argc, char* argv[]) {
  if (!strcmp(argv[1], "1"))
  {
    return exec("custom-child 2");
  }
  else 
  {
    while (1) {}
    return 69;
  }
}