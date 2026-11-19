/**
 * @file test_portal_assets.c
 * @brief Integrity tests over the embedded portal asset table (W02, issue #69).
 *
 * Runs on the host and on the device: the table is generated at build time
 * from the committed web/dist bundle, so these tests pin the served bundle
 * itself — the paths, the MIME types and the "no external origin" rule of
 * the local-portal spec.
 */

#include <string.h>

#include "portal_assets.h"
#include "unity.h"

/** True when @p needle occurs in @p haystack (bounded memcmp scan, no memmem). */
static bool contains(const uint8_t *haystack, size_t hay_len, const char *needle)
{
    const size_t needle_len = strlen(needle);
    if (needle_len == 0u || hay_len < needle_len) {
        return false;
    }
    for (size_t i = 0; i + needle_len <= hay_len; ++i) {
        if (memcmp(&haystack[i], needle, needle_len) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * Assert that every `src="`/`href="` attribute value in the bytes starts
 * with `.` or `/` (relative or rooted: never an external origin), and
 * return how many attribute values were checked.
 */
static size_t assert_attr_values_local(const uint8_t *bytes, size_t len)
{
    static const char *k_attrs[] = {"src=\"", "href=\""};
    size_t checked = 0u;

    for (size_t a = 0; a < sizeof(k_attrs) / sizeof(k_attrs[0]); ++a) {
        const size_t attr_len = strlen(k_attrs[a]);
        for (size_t i = 0; i + attr_len < len; ++i) {
            if (memcmp(&bytes[i], k_attrs[a], attr_len) != 0) {
                continue;
            }
            const char value = (char)bytes[i + attr_len];
            TEST_ASSERT_TRUE_MESSAGE(value == '.' || value == '/',
                                      "asset attribute points off-origin");
            ++checked;
        }
    }
    return checked;
}

TEST_CASE("the asset table carries exactly the portal bundle", "[portal_assets]")
{
    TEST_ASSERT_EQUAL_UINT32(3u, (uint32_t)k_portal_assets_count);

    const portal_asset_t *index = portal_assets_find("/");
    TEST_ASSERT_NOT_NULL(index);
    TEST_ASSERT_EQUAL_STRING("text/html", index->mime);
    TEST_ASSERT_TRUE(index->len > 0u);
    TEST_ASSERT_NOT_NULL(index->bytes);

    const portal_asset_t *app_js = portal_assets_find("/app.js");
    TEST_ASSERT_NOT_NULL(app_js);
    TEST_ASSERT_EQUAL_STRING("application/javascript", app_js->mime);
    TEST_ASSERT_TRUE(app_js->len > 0u);
    TEST_ASSERT_NOT_NULL(app_js->bytes);

    const portal_asset_t *css = portal_assets_find("/assets/index.css");
    TEST_ASSERT_NOT_NULL(css);
    TEST_ASSERT_EQUAL_STRING("text/css", css->mime);
    TEST_ASSERT_TRUE(css->len > 0u);
    TEST_ASSERT_NOT_NULL(css->bytes);

    TEST_ASSERT_NULL(portal_assets_find("/no-such-asset.js"));
    TEST_ASSERT_NULL(portal_assets_find(NULL));
}

TEST_CASE("served assets reference no external origin", "[portal_assets]")
{
    for (size_t i = 0; i < k_portal_assets_count; ++i) {
        const portal_asset_t *asset = &k_portal_assets[i];
        TEST_ASSERT_FALSE_MESSAGE(contains(asset->bytes, asset->len, "fetch(\"http"),
                                  "asset fetches an external origin");
        TEST_ASSERT_FALSE_MESSAGE(contains(asset->bytes, asset->len, "fetch('http"),
                                  "asset fetches an external origin");
        TEST_ASSERT_FALSE_MESSAGE(contains(asset->bytes, asset->len, "@import url(http"),
                                  "asset imports an external origin");
    }

    const portal_asset_t *index = portal_assets_find("/");
    TEST_ASSERT_NOT_NULL(index);
    TEST_ASSERT_FALSE_MESSAGE(contains(index->bytes, index->len, "http://"),
                              "index.html references an external origin");
    /* The check must have seen the two attributes of the committed
     * index.html, so it cannot pass vacuously. */
    TEST_ASSERT_GREATER_THAN_UINT32(1u,
                                    (uint32_t)assert_attr_values_local(index->bytes, index->len));
}
