#include "yaza_runtime_v1.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    const int64_t count = yaza_rt_v1_processor_count();
    assert(count >= 1);
    assert(count <= 1048576);
    puts("processor-count-ok");
    return 0;
}
