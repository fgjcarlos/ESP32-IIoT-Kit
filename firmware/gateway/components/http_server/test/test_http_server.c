#include "http_server.h"

#include "unity.h"

#include <string.h>

TEST_CASE("http_server contract entry points return success", "[http_server]")
{
    TEST_ASSERT_EQUAL(ESP_OK, http_server_start());
    TEST_ASSERT_EQUAL(ESP_OK, http_server_stop());
}

TEST_CASE("http_server brand payload carries the product identity", "[http_server]")
{
    char payload[HTTP_SERVER_BRAND_PAYLOAD_MAX_LEN] = {0};

    /* Issue #12: the API has to expose the name, the model, the version and
     * the revision of the product this firmware was built for. */
    TEST_ASSERT_EQUAL(ESP_OK, http_server_brand_payload(payload, sizeof(payload)));

    TEST_ASSERT_NOT_NULL(strstr(payload, "\"brand\":\""));
    TEST_ASSERT_NOT_NULL(strstr(payload, "\"model\":\""));
    TEST_ASSERT_NOT_NULL(strstr(payload, "\"version\":\""));
    TEST_ASSERT_NOT_NULL(strstr(payload, "\"revision\":\""));
    TEST_ASSERT_NOT_NULL(strstr(payload, "\"channel\":\""));

    /* The payload is deterministic: the same build always answers the same
     * bytes, so a client may cache it. */
    char again[HTTP_SERVER_BRAND_PAYLOAD_MAX_LEN] = {0};
    TEST_ASSERT_EQUAL(ESP_OK, http_server_brand_payload(again, sizeof(again)));
    TEST_ASSERT_EQUAL_STRING(payload, again);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      http_server_brand_payload(payload, 8u));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, http_server_brand_payload(NULL, 0u));
}
