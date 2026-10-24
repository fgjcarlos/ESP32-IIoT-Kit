// Host shim for the header the brand_profile component generates at
// configure time on the device.
//
// On the ESP32-S3 the build embeds the logo of the selected profile and
// writes its geometry here. The host build has no embedded asset, so the
// shim declares a tiny placeholder with the same shape: the code under test
// takes the same path in both places, which is what makes the host run a
// meaningful rehearsal of the device behaviour.
#pragma once

#define BRAND_BUILD_PROFILE_ID   "iiot-kit"
#define BRAND_BUILD_REVISION     "hosttest0"
#define BRAND_LOGO_WIDTH         2u
#define BRAND_LOGO_HEIGHT        2u
#define BRAND_ASSET_BYTES        8u
#define BRAND_ASSET_VERSION      "1"
#define BRAND_ASSET_BUDGET_BYTES 32768u
