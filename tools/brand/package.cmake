# tools/brand/package.cmake
#
# Name the application image after the brand profile and write its metadata
# manifest.
#
# The naming convention is the one implemented by
# `brand_profile_build_artifact_name()` in the firmware:
#
#     <brand>-<model>-v<version>-<channel>-<revision>.bin
#
# Every token is slugified (lowercase ASCII letters, digits and dots are kept,
# everything else becomes a dash), so the name is stable, comparable and safe
# to use as a file name on every platform.
#
# Beside the image the script writes `<name>.manifest.json` with the metadata
# of the build: the brand identity, the revision, the size and the SHA-256 of
# the image. The manifest is what makes a release traceable: the file name
# says which product it is, the manifest says exactly which bytes it is.
#
# The script is invoked by the gateway project as part of the build, so the
# branded artifact is always produced together with the image it describes.

if(NOT BRAND_SOURCE_BIN)
    message(FATAL_ERROR "BRAND_SOURCE_BIN is required")
endif()
if(NOT EXISTS "${BRAND_SOURCE_BIN}")
    message(FATAL_ERROR "Brand artifact source not found: ${BRAND_SOURCE_BIN}")
endif()
if(NOT BRAND_CMAKE_MODULE)
    message(FATAL_ERROR "BRAND_CMAKE_MODULE is required")
endif()

include(${BRAND_CMAKE_MODULE})

brand_profile_artifact_name("${BRAND_NAME}"
                            "${BRAND_MODEL}"
                            "${BRAND_VERSION}"
                            "${BRAND_CHANNEL}"
                            "${BRAND_REVISION}"
                            BRAND_ARTIFACT_NAME)

set(BRAND_TARGET_BIN "${CMAKE_CURRENT_BINARY_DIR}/${BRAND_ARTIFACT_NAME}")
set(BRAND_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/${BRAND_ARTIFACT_NAME}.manifest.json")

# The branded copy is the artifact of record; the generic one stays in place
# so `idf.py flash` keeps working.
configure_file("${BRAND_SOURCE_BIN}" "${BRAND_TARGET_BIN}" COPYONLY)

file(SHA256 "${BRAND_TARGET_BIN}" BRAND_SHA256)
file(SIZE "${BRAND_TARGET_BIN}" BRAND_SIZE)

string(TIMESTAMP BRAND_BUILD_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)

# A JSON string needs its quotes and backslashes escaped. The values that
# reach this point are brand configuration, not free-form user input, but the
# escaping keeps the manifest valid whatever the profile says.
function(brand_json_escape input out_var)
    string(REPLACE "\\" "\\\\" _escaped "${input}")
    string(REPLACE "\"" "\\\"" _escaped "${_escaped}")
    set(${out_var} "${_escaped}" PARENT_SCOPE)
endfunction()

brand_json_escape("${BRAND_NAME}" _brand_json_name)
brand_json_escape("${BRAND_MODEL}" _brand_json_model)
brand_json_escape("${BRAND_VERSION}" _brand_json_version)
brand_json_escape("${BRAND_CHANNEL}" _brand_json_channel)
brand_json_escape("${BRAND_REVISION}" _brand_json_revision)
brand_json_escape("${BRAND_PROFILE_ID}" _brand_json_profile)
brand_json_escape("${BRAND_SUPPORT_URL}" _brand_json_support_url)
brand_json_escape("${BRAND_AP_SSID}" _brand_json_ssid)
brand_json_escape("${BRAND_ARTIFACT_NAME}" _brand_json_artifact)

file(WRITE "${BRAND_MANIFEST}"
"{
  \"artifact\": \"${_brand_json_artifact}\",
  \"profile\": \"${_brand_json_profile}\",
  \"brand\": \"${_brand_json_name}\",
  \"model\": \"${_brand_json_model}\",
  \"version\": \"${_brand_json_version}\",
  \"channel\": \"${_brand_json_channel}\",
  \"revision\": \"${_brand_json_revision}\",
  \"support_url\": \"${_brand_json_support_url}\",
  \"ap_ssid\": \"${_brand_json_ssid}\",
  \"size_bytes\": ${BRAND_SIZE},
  \"sha256\": \"${BRAND_SHA256}\",
  \"built_at\": \"${BRAND_BUILD_TIME}\"
}
")

message(STATUS "Brand artifact: ${BRAND_TARGET_BIN} (${BRAND_SIZE} bytes)")
message(STATUS "Brand manifest: ${BRAND_MANIFEST}")
