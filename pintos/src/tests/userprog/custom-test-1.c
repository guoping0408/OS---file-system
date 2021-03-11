/* In this test, we will wait on the grand child process. This should not be allowed. */

#include "tests/lib.h"
#include "tests/main.h"
#include <string.h>
#include <syscall.h>

void test_main(void) {
    int grandchild_pid = wait(exec("custom-child 1"));
    return wait(grandchild_pid);
}
