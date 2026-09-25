#include "unity.h"
#include "board_profile.h"

#define EXPECTED_FLASH_SIZE_BYTES (16U * 1024U * 1024U)
#define EXPECTED_PSRAM_SIZE_BYTES (8U * 1024U * 1024U)

TEST_CASE("Connected gateway matches the selected ESP32-S3 memory profile", "[board_profile]")
{
    board_profile_t profile;

    TEST_ASSERT_EQUAL(ESP_OK, board_profile_read(&profile));
    TEST_ASSERT_EQUAL(CHIP_ESP32S3, profile.chip_model);
    TEST_ASSERT_EQUAL_UINT32(EXPECTED_FLASH_SIZE_BYTES, profile.flash_size_bytes);
    TEST_ASSERT_EQUAL_UINT32(EXPECTED_PSRAM_SIZE_BYTES, profile.psram_size_bytes);
}
