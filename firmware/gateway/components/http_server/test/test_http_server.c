#include "http_server.h"

#include "unity.h"

TEST_CASE("http_server contract entry points return success", "[http_server]")
{
    TEST_ASSERT_EQUAL(ESP_OK, http_server_start());
    TEST_ASSERT_EQUAL(ESP_OK, http_server_stop());
}
