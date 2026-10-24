#include "unity.h"

#include "brand_profile.h"

#include <string.h>

/* A synthetic profile, so the deterministic rules are asserted against
 * known inputs instead of against whatever the build happened to select. */
static void brand_test_fill(brand_profile_t *profile)
{
    memset(profile, 0, sizeof(*profile));

    profile->profile_id = "acme-industrial";
    profile->brand_name = "Acme Industrial";
    profile->model = "AX-200";
    profile->support_url = "https://support.acme.example";
    profile->ap_ssid = "Acme-AX200";
    profile->firmware_version = "1.2.0";
    profile->ota_channel = "beta";
    profile->revision = "abc12345";
    profile->color_background = 0x101820u;
    profile->color_foreground = 0xFFFFFFu;
    profile->color_accent = 0xFF7A00u;
}

TEST_CASE("Brand colours parse from the Kconfig hex notation", "[brand_profile]")
{
    uint32_t rgb = 0u;

    TEST_ASSERT_EQUAL(ESP_OK, brand_profile_parse_color("0B1020", &rgb));
    TEST_ASSERT_EQUAL_HEX32(0x0B1020u, rgb);

    /* Lowercase and a leading '#' are accepted too: the same colour must
     * always produce the same value. */
    TEST_ASSERT_EQUAL(ESP_OK, brand_profile_parse_color("#0b1020", &rgb));
    TEST_ASSERT_EQUAL_HEX32(0x0B1020u, rgb);

    TEST_ASSERT_EQUAL(ESP_OK, brand_profile_parse_color("FF7A00", &rgb));
    TEST_ASSERT_EQUAL_HEX32(0xFF7A00u, rgb);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, brand_profile_parse_color("0B10", &rgb));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, brand_profile_parse_color("0B10ZZ", &rgb));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, brand_profile_parse_color(NULL, &rgb));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, brand_profile_parse_color("0B1020", NULL));
}

TEST_CASE("Brand colours format back to uppercase hex", "[brand_profile]")
{
    char text[BRAND_PROFILE_COLOR_STR_LEN] = {0};

    TEST_ASSERT_EQUAL(ESP_OK, brand_profile_format_color(0x2E8BFFu, text, sizeof(text)));
    TEST_ASSERT_EQUAL_STRING("2E8BFF", text);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      brand_profile_format_color(0x2E8BFFu, text, 4u));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      brand_profile_format_color(0x2E8BFFu, NULL, sizeof(text)));
}

TEST_CASE("Brand profile validation rejects incoherent identities", "[brand_profile]")
{
    brand_profile_t profile;

    brand_test_fill(&profile);
    TEST_ASSERT_EQUAL(ESP_OK, brand_profile_validate(&profile));

    /* A profile without a name is not publishable. */
    brand_profile_t broken = profile;
    broken.brand_name = "";
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, brand_profile_validate(&broken));

    /* The support URL has to be absolute. */
    broken = profile;
    broken.support_url = "support.acme.example";
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, brand_profile_validate(&broken));

    /* The SSID must fit the 802.11 limit. */
    broken = profile;
    broken.ap_ssid = "0123456789012345678901234567890123";
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, brand_profile_validate(&broken));

    /* A logo must match its declared dimensions. */
    broken = profile;
    broken.logo = (const uint8_t *)&profile;
    broken.logo_size = 10u;
    broken.logo_width = 64u;
    broken.logo_height = 64u;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, brand_profile_validate(&broken));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, brand_profile_validate(NULL));
}

TEST_CASE("Brand slugify keeps only safe characters", "[brand_profile]")
{
    char slug[64] = {0};

    TEST_ASSERT_EQUAL(ESP_OK, brand_profile_slugify("Acme Industrial", slug, sizeof(slug)));
    TEST_ASSERT_EQUAL_STRING("acme-industrial", slug);

    TEST_ASSERT_EQUAL(ESP_OK, brand_profile_slugify("IIoT-Kit", slug, sizeof(slug)));
    TEST_ASSERT_EQUAL_STRING("iiot-kit", slug);

    TEST_ASSERT_EQUAL(ESP_OK, brand_profile_slugify("  spaced  out  ", slug, sizeof(slug)));
    TEST_ASSERT_EQUAL_STRING("spaced-out", slug);

    TEST_ASSERT_EQUAL(ESP_OK, brand_profile_slugify("v1.2.0", slug, sizeof(slug)));
    TEST_ASSERT_EQUAL_STRING("v1.2.0", slug);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      brand_profile_slugify("acme", slug, 3u));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, brand_profile_slugify(NULL, slug, sizeof(slug)));
}

TEST_CASE("Brand artifact name follows the reproducible convention", "[brand_profile]")
{
    brand_profile_t profile;
    char name[BRAND_PROFILE_ARTIFACT_MAX_LEN] = {0};

    brand_test_fill(&profile);

    TEST_ASSERT_EQUAL(ESP_OK,
                      brand_profile_build_artifact_name(&profile, name, sizeof(name)));
    TEST_ASSERT_EQUAL_STRING("acme-industrial-ax-200-v1.2.0-beta-abc12345.bin", name);

    /* The same profile always produces the same name: that is what makes
     * the artifact traceable. */
    char again[BRAND_PROFILE_ARTIFACT_MAX_LEN] = {0};
    TEST_ASSERT_EQUAL(ESP_OK,
                      brand_profile_build_artifact_name(&profile, again, sizeof(again)));
    TEST_ASSERT_EQUAL_STRING(name, again);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      brand_profile_build_artifact_name(&profile, name, 8u));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      brand_profile_build_artifact_name(NULL, name, sizeof(name)));
}

TEST_CASE("Brand JSON payload is deterministic and carries the identity", "[brand_profile]")
{
    brand_profile_t profile;
    char payload[BRAND_PROFILE_JSON_MAX_LEN] = {0};
    char again[BRAND_PROFILE_JSON_MAX_LEN] = {0};

    brand_test_fill(&profile);

    TEST_ASSERT_EQUAL(ESP_OK,
                      brand_profile_format_json(&profile, payload, sizeof(payload)));

    /* The payload names the product, its version and its revision: the
     * three fields the API has to expose. */
    TEST_ASSERT_NOT_NULL(strstr(payload, "\"brand\":\"Acme Industrial\""));
    TEST_ASSERT_NOT_NULL(strstr(payload, "\"model\":\"AX-200\""));
    TEST_ASSERT_NOT_NULL(strstr(payload, "\"version\":\"1.2.0\""));
    TEST_ASSERT_NOT_NULL(strstr(payload, "\"revision\":\"abc12345\""));
    TEST_ASSERT_NOT_NULL(strstr(payload, "\"channel\":\"beta\""));
    TEST_ASSERT_NOT_NULL(strstr(payload, "\"background\":\"101820\""));
    TEST_ASSERT_NOT_NULL(strstr(payload, "\"accent\":\"FF7A00\""));

    /* Same profile, same bytes. */
    TEST_ASSERT_EQUAL(ESP_OK,
                      brand_profile_format_json(&profile, again, sizeof(again)));
    TEST_ASSERT_EQUAL_STRING(payload, again);

    /* The announced size must be the size actually needed. */
    TEST_ASSERT_EQUAL_UINT32((uint32_t)strlen(payload) + 1u,
                             (uint32_t)brand_profile_json_size(&profile));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      brand_profile_format_json(&profile, payload, 8u));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      brand_profile_format_json(NULL, payload, sizeof(payload)));
}

TEST_CASE("Brand summary line carries the identity for the display", "[brand_profile]")
{
    brand_profile_t profile;
    char summary[BRAND_PROFILE_ARTIFACT_MAX_LEN] = {0};

    brand_test_fill(&profile);

    TEST_ASSERT_EQUAL(ESP_OK,
                      brand_profile_summary(&profile, summary, sizeof(summary)));
    TEST_ASSERT_EQUAL_STRING("Acme Industrial AX-200 v1.2.0 abc12345", summary);
}

TEST_CASE("The compiled brand profile is coherent", "[brand_profile]")
{
    const brand_profile_t *profile = brand_profile_get();

    TEST_ASSERT_NOT_NULL(profile);
    TEST_ASSERT_EQUAL(ESP_OK, brand_profile_validate(profile));

    /* The profile identifier, the brand name and the model are never empty:
     * they are what the LCD, the API and the logs show. */
    TEST_ASSERT_NOT_NULL(profile->profile_id);
    TEST_ASSERT_TRUE(strlen(profile->profile_id) > 0u);
    TEST_ASSERT_NOT_NULL(profile->brand_name);
    TEST_ASSERT_TRUE(strlen(profile->brand_name) > 0u);
    TEST_ASSERT_NOT_NULL(profile->model);
    TEST_ASSERT_TRUE(strlen(profile->model) > 0u);

    /* The revision is the build revision; it is never empty, even when the
     * build could not read it from the repository. */
    TEST_ASSERT_NOT_NULL(profile->revision);
    TEST_ASSERT_TRUE(strlen(profile->revision) > 0u);
}

TEST_CASE("The compiled brand profile embeds the logo of its profile", "[brand_profile]")
{
    const brand_profile_t *profile = brand_profile_get();

    TEST_ASSERT_NOT_NULL(profile->logo);
    TEST_ASSERT_TRUE(profile->logo_width > 0u);
    TEST_ASSERT_TRUE(profile->logo_height > 0u);

    /* RGB565: two bytes per pixel. */
    TEST_ASSERT_EQUAL_UINT32((uint32_t)profile->logo_width *
                                  (uint32_t)profile->logo_height * 2u,
                             (uint32_t)profile->logo_size);
}

TEST_CASE("The boot log publishes the identity", "[brand_profile]")
{
    /* Issue #12: the identity has to be visible in the logs. The function
     * logs one line and returns; the test asserts it succeeds and does not
     * abort, which is what a corrupted profile would do. */
    TEST_ASSERT_EQUAL(ESP_OK, brand_profile_log());
}

TEST_CASE("Manufacturing secrets never leak into the public identity", "[brand_profile]"){
    const brand_secrets_t *secrets = brand_secrets_get();

    TEST_ASSERT_NOT_NULL(secrets);

    /* The presence flag is all the firmware may expose. The values stay
     * inside the component: the API payload, the logs and the LCD never
     * carry a secret. */
    if (brand_secrets_present()) {
        TEST_ASSERT_NOT_NULL(secrets->provisioning_key);
    }

    char payload[BRAND_PROFILE_JSON_MAX_LEN] = {0};
    TEST_ASSERT_EQUAL(ESP_OK,
                      brand_profile_format_json(brand_profile_get(),
                                                payload, sizeof(payload)));
    TEST_ASSERT_NULL(strstr(payload, "provisioning"));
    TEST_ASSERT_NULL(strstr(payload, "token"));
    TEST_ASSERT_NULL(strstr(payload, "password"));
}
