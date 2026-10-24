// Host shim: the Unity runner calls esp_restart() when a test asks for a
// device reset. On the host that means "abort", because there is no device.
#pragma once

void esp_restart(void);
