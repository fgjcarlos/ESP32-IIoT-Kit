#include "unity.h"
#include "assets_brand.h"

#include <string.h>

TEST_CASE("Assets brand placeholder label matches design constant",
          "[assets_brand]")
{
    /* The label is the single source of truth for the brand placeholder text.
     * Locking it in a test prevents accidental drift if a future change
     * touches the header constant without intending to update the test. */
    TEST_ASSERT_EQUAL_STRING("IIoT-Kit", ASSETS_BRAND_PLACEHOLDER_LABEL);
    TEST_ASSERT_EQUAL_INT32(8, (long)strlen(ASSETS_BRAND_PLACEHOLDER_LABEL));
}

TEST_CASE("Assets brand init returns ESP_OK", "[assets_brand]")
{
    TEST_ASSERT_EQUAL(ESP_OK, assets_brand_init());
}