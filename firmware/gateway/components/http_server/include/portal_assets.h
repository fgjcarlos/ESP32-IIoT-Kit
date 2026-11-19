#pragma once

/**
 * @file portal_assets.h
 * @brief Served-from-flash asset table for the local portal (W02, issue #69).
 *
 * The table itself is generated from the committed `web/dist` bundle by
 * `tools/web/embed_assets.py` at build time; this header is the contract the
 * http_server glue and the tests code against.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** One embedded asset: served path, MIME type and exact bytes. */
typedef struct {
    const char *path;   /**< URL path the asset answers, "/" for index. */
    const char *mime;   /**< Content-Type served with the asset. */
    size_t len;         /**< Asset length in bytes. */
    const uint8_t *bytes; /**< Asset bytes, embedded in the firmware image. */
} portal_asset_t;

/** Generated table of the portal bundle. */
extern const portal_asset_t k_portal_assets[];

/** Number of entries in k_portal_assets. */
extern const size_t k_portal_assets_count;

/**
 * @brief Find an embedded asset by its served path.
 *
 * @return The matching entry, or NULL when the path is not embedded.
 */
const portal_asset_t *portal_assets_find(const char *path);

#ifdef __cplusplus
}
#endif
