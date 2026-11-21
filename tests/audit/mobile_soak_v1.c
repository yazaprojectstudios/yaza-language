#include "yaza_mobile_v1.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__APPLE__)
#include <mach/mach.h>
#else
#include <sys/resource.h>
#endif

static double current_rss_mib(void) {
#if defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS) {
        return (double)info.resident_size / (1024.0 * 1024.0);
    }
    return 0.0;
#else
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == 0) {
        return (double)ru.ru_maxrss / 1024.0;
    }
    return 0.0;
#endif
}

int main(void) {
    YazaMobileHostV1 host;
    memset(&host, 0, sizeof(host));
    host.struct_size = (uint32_t)sizeof(host);
    host.abi_version = YAZA_MOBILE_V1_ABI_VERSION;

    const double rss_start = current_rss_mib();

    /* Phase A: 2,000 full engine lifecycle cycles */
    const uint8_t inc_payload[] = "1";
    for (int i = 0; i < 2000; ++i) {
        YazaMobileEngineHandleV1 eng = 0;
        assert(yaza_mobile_v1_create(&host, &eng) == YAZA_MOBILE_V1_OK);
        assert(eng > 0);
        YazaMobileBufferV1 out = {0};
        YazaMobileView view = {inc_payload, sizeof(inc_payload) - 1};
        assert(yaza_mobile_v1_dispatch(eng, YAZA_MOBILE_EVENT_V1_INCREMENT, view, &out) == YAZA_MOBILE_V1_OK);
        assert(out.length == 1 && out.data[0] == '1');
        assert(yaza_mobile_v1_buffer_release(&out) == YAZA_MOBILE_V1_OK);
        assert(yaza_mobile_v1_destroy(eng) == YAZA_MOBILE_V1_OK);
    }

    const double rss_mid = current_rss_mib();

    /* Phase B: 20,000 dispatches with 1 B, 1 KiB, and 32 KiB payloads */
    uint8_t *buf_32k = (uint8_t *)malloc(32768);
    assert(buf_32k != NULL);
    memset(buf_32k, 'A', 32768);

    YazaMobileEngineHandleV1 eng = 0;
    assert(yaza_mobile_v1_create(&host, &eng) == YAZA_MOBILE_V1_OK);
    for (int i = 0; i < 20000; ++i) {
        YazaMobileBufferV1 out = {0};
        uint32_t ev = YAZA_MOBILE_EVENT_V1_SNAPSHOT;
        size_t len = 1;
        const uint8_t *ptr = buf_32k;
        if (i % 3 == 0) {
            ev = YAZA_MOBILE_EVENT_V1_INCREMENT;
            ptr = inc_payload;
            len = 1;
        } else if (i % 3 == 1) {
            len = 1024;
        } else {
            len = 32768;
        }
        YazaMobileView view = {ptr, (uint64_t)len};
        assert(yaza_mobile_v1_dispatch(eng, ev, view, &out) == YAZA_MOBILE_V1_OK);
        assert(out.data != NULL && out.length > 0);
        assert(yaza_mobile_v1_buffer_release(&out) == YAZA_MOBILE_V1_OK);
    }
    assert(yaza_mobile_v1_destroy(eng) == YAZA_MOBILE_V1_OK);
    free(buf_32k);

    const double rss_end = current_rss_mib();
    printf("mobile-soak-v1: 2000 create/destroy + 20000 dispatches (1B..32KiB) PASSED | RSS MiB: start=%.2f mid=%.2f end=%.2f\n",
           rss_start, rss_mid, rss_end);
    return 0;
}
