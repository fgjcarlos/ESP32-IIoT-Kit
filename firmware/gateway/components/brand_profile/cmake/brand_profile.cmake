# Resolution of the active brand profile and enforcement of the brand asset
# flash budget.
#
# The Kconfig choice `BRAND_PROFILE` and the on-disk directories under
# `profiles/` meet here. This is the only place where the two are mapped, so
# adding a profile means adding one branch in `brand_profile_resolve` plus one
# directory under `profiles/`.
#
# The module is included both by the `brand_profile` component (which embeds
# the assets) and by the gateway project (which names the artifact), so the
# two always agree on which profile is active.

# brand_profile_resolve(<profiles_root> <out_id> <out_dir>)
#
# Maps the selected Kconfig choice onto the profile identifier and the
# directory holding its versioned assets.
function(brand_profile_resolve profiles_root out_id out_dir)
    if(CONFIG_BRAND_PROFILE_ACME_INDUSTRIAL)
        set(_brand_id "acme-industrial")
    elseif(CONFIG_BRAND_PROFILE_IIOT_KIT)
        set(_brand_id "iiot-kit")
    else()
        message(FATAL_ERROR
            "No brand profile is selected. Enable CONFIG_BRAND_PROFILE_IIOT_KIT or "
            "CONFIG_BRAND_PROFILE_ACME_INDUSTRIAL in the Kconfig menu "
            "'Brand profile (white-label)'.")
    endif()

    set(${out_id} "${_brand_id}" PARENT_SCOPE)
    set(${out_dir} "${profiles_root}/${_brand_id}" PARENT_SCOPE)
endfunction()

# brand_profile_manifest_value(<manifest_path> <key> <out_var>)
#
# Reads one `key=value` entry of a profile manifest. Lines starting with '#'
# and blank lines are ignored.
function(brand_profile_manifest_value manifest_path key out_var)
    if(NOT EXISTS "${manifest_path}")
        message(FATAL_ERROR "Brand profile manifest not found: ${manifest_path}")
    endif()

    file(STRINGS "${manifest_path}" _brand_manifest_lines)

    set(_brand_value "")
    foreach(_brand_line IN LISTS _brand_manifest_lines)
        string(STRIP "${_brand_line}" _brand_line)
        if(_brand_line STREQUAL "" OR _brand_line MATCHES "^#")
            continue()
        endif()
        if(_brand_line MATCHES "^${key}=(.*)$")
            set(_brand_value "${CMAKE_MATCH_1}")
        endif()
    endforeach()

    set(${out_var} "${_brand_value}" PARENT_SCOPE)
endfunction()

# brand_profile_check_budget(<profile_id> <asset_bytes> <budget> <font_reserve>)
#
# The brand assets and the font allowance must fit the declared budget. The
# check is a hard build failure on purpose: an oversized logo must never
# reach a device.
function(brand_profile_check_budget profile_id asset_bytes budget font_reserve)
    if(font_reserve GREATER budget)
        message(FATAL_ERROR
            "Brand font reserve (${font_reserve} bytes) is larger than the brand "
            "asset budget (${budget} bytes). Lower CONFIG_BRAND_FONT_RESERVE_BYTES "
            "or raise CONFIG_BRAND_ASSET_BUDGET_BYTES.")
    endif()

    math(EXPR _brand_total "${asset_bytes} + ${font_reserve}")
    if(_brand_total GREATER budget)
        message(FATAL_ERROR
            "Brand assets of profile '${profile_id}' exceed the flash budget: "
            "logo ${asset_bytes} B + font reserve ${font_reserve} B = ${_brand_total} B "
            "> ${budget} B. Shrink the logo or raise CONFIG_BRAND_ASSET_BUDGET_BYTES.")
    endif()

    message(STATUS
        "Brand profile '${profile_id}': ${asset_bytes} B of assets + ${font_reserve} B "
        "of font reserve = ${_brand_total} B of a ${budget} B budget")
endfunction()

# brand_profile_git_revision(<out_var>)
#
# The build revision is the short commit SHA of the working tree. A dirty
# tree is marked with a '-dirty' suffix so an artifact can never claim to be
# reproducible when it is not.
function(brand_profile_git_revision out_var)
    set(_brand_revision "unknown")

    find_package(Git QUIET)
    if(GIT_EXECUTABLE)
        execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=8 HEAD
                        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                        OUTPUT_VARIABLE _brand_sha
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_QUIET
                        RESULT_VARIABLE _brand_git_result)

        if(_brand_git_result EQUAL 0 AND _brand_sha)
            set(_brand_revision "${_brand_sha}")

            execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain
                            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                            OUTPUT_VARIABLE _brand_dirty
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                            ERROR_QUIET
                            RESULT_VARIABLE _brand_dirty_result)

            if(_brand_dirty_result EQUAL 0 AND NOT _brand_dirty STREQUAL "")
                set(_brand_revision "${_brand_revision}-dirty")
            endif()
        endif()
    endif()

    set(${out_var} "${_brand_revision}" PARENT_SCOPE)
endfunction()

# brand_profile_slugify(<in> <out_var>)
#
# Mirrors `brand_profile_slugify()` in `brand_profile.c`: lowercase ASCII
# letters, digits and dots are kept, every other character becomes a dash
# and repeated dashes are collapsed.
function(brand_profile_slugify input out_var)
    string(REGEX REPLACE "[^a-zA-Z0-9.]+" "-" _slug "${input}")
    string(REGEX REPLACE "-+" "-" _slug "${_slug}")
    string(REGEX REPLACE "^-+" "" _slug "${_slug}")
    string(REGEX REPLACE "-+$" "" _slug "${_slug}")
    string(TOLOWER "${_slug}" _slug)
    set(${out_var} "${_slug}" PARENT_SCOPE)
endfunction()

# brand_profile_artifact_name(<brand> <model> <version> <channel> <revision> <out_var>)
#
# The reproducible naming convention:
#
#     <brand>-<model>-v<version>-<channel>-<revision>.bin
function(brand_profile_artifact_name brand model version channel revision out_var)
    brand_profile_slugify("${brand}" _slug_brand)
    brand_profile_slugify("${model}" _slug_model)
    brand_profile_slugify("${version}" _slug_version)
    brand_profile_slugify("${channel}" _slug_channel)
    brand_profile_slugify("${revision}" _slug_revision)

    set(${out_var}
        "${_slug_brand}-${_slug_model}-v${_slug_version}-${_slug_channel}-${_slug_revision}.bin"
        PARENT_SCOPE)
endfunction()
