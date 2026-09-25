#include "unity.h"
#include "board_profile.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#define EXPECTED_FLASH_SIZE_BYTES (16U * 1024U * 1024U)
#define EXPECTED_PSRAM_SIZE_BYTES (8U * 1024U * 1024U)
#define FRAMEBUFFER_SIZE_BYTES (170U * 320U * 2U)

static const char *TAG = "board_profile";

TEST_CASE("Connected gateway matches the selected ESP32-S3 memory profile", "[board_profile]")
{
    board_profile_t profile;

    TEST_ASSERT_EQUAL(ESP_OK, board_profile_read(&profile));
    TEST_ASSERT_EQUAL(CHIP_ESP32S3, profile.chip_model);
    TEST_ASSERT_EQUAL_UINT32(EXPECTED_FLASH_SIZE_BYTES, profile.flash_size_bytes);
    TEST_ASSERT_EQUAL_UINT32(EXPECTED_PSRAM_SIZE_BYTES, profile.psram_size_bytes);
}

TEST_CASE("PSRAM allocates one RGB565 framebuffer-sized 8-bit block", "[board_profile]")
{
    const size_t free_internal_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const size_t largest_psram_block = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

    ESP_LOGI(TAG,
             "Heap before framebuffer allocation: internal=%u bytes, PSRAM=%u bytes, "
             "largest PSRAM block=%u bytes",
             (unsigned int)free_internal_heap,
             (unsigned int)free_psram,
             (unsigned int)largest_psram_block);

    void *framebuffer = heap_caps_malloc(FRAMEBUFFER_SIZE_BYTES,
                                        MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

    TEST_ASSERT_NOT_NULL(framebuffer);
    heap_caps_free(framebuffer);
}
