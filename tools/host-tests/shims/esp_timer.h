// Host shim: a monotonic microsecond counter backed by the host clock.
#pragma once

#include <stdint.h>

int64_t esp_timer_get_time(void);
