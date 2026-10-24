// Host-side test runner for the gateway's portable logic.
//
// It compiles the SAME Unity test files that the on-device test project runs
// (firmware/gateway/components/sparkplug_b/test/*.c) against the real
// component sources, so the assertions are not duplicated.
//
// Why this exists: the gateway's Sparkplug B logic (payload codec, command
// translation, device registry) is plain C and can be executed natively. The
// ESP32-S3 carrier used during development could not boot from flash (the
// GPIO46 strap was held low), so the on-device runner could not be used to
// execute the suite.

#include <stdio.h>

#include "unity.h"

void unity_run_all_tests(void);

int main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    int failures = UNITY_END();
    printf("\nhost tests finished: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
