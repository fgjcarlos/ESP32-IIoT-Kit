/**
 * @file portal_assets.c
 * @brief Lookup over the generated embedded portal asset table.
 */

#include "portal_assets.h"

#include <string.h>

const portal_asset_t *portal_assets_find(const char *path)
{
    if (path == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < k_portal_assets_count; ++i) {
        if (strcmp(k_portal_assets[i].path, path) == 0) {
            return &k_portal_assets[i];
        }
    }
    return NULL;
}
