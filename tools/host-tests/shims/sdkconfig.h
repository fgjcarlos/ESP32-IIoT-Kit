// Host shim for the ESP-IDF generated sdkconfig.h. It only has to satisfy
// the Unity configuration header and the modules under test that read their
// configuration from it.
#pragma once

#define CONFIG_UNITY_ENABLE_FLOAT 1
#define CONFIG_UNITY_ENABLE_DOUBLE 1
#define CONFIG_UNITY_ENABLE_IDF_TEST_RUNNER 1

// --- brand profile (issue #12) --------------------------------------------
//
// The host build compiles the real brand_profile component, so it needs the
// same public configuration the firmware gets from the Kconfig. The values
// below mirror the reference profile of the repository.
#define CONFIG_BRAND_NAME "IIoT-Kit"
#define CONFIG_BRAND_MODEL "GW-S3-1"
#define CONFIG_BRAND_SUPPORT_URL "https://example.org/iiot-kit/support"
#define CONFIG_BRAND_AP_SSID "IIoT-Kit-GW"
#define CONFIG_BRAND_OTA_CHANNEL "stable"
#define CONFIG_BRAND_FIRMWARE_VERSION "1.0.0"
#define CONFIG_BRAND_COLOR_BACKGROUND "0B1020"
#define CONFIG_BRAND_COLOR_FOREGROUND "E8EEF7"
#define CONFIG_BRAND_COLOR_ACCENT "2E8BFF"
