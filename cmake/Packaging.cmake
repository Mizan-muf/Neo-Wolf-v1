# Packaging (Roadmap Phase 9): the application icon, the macOS app bundle and its Info.plist,
# the Windows version/icon resource, the README.txt of the package, install rules and CPack.
# Included from CMakeLists.txt after the neo_wolf target. docs/RELEASE.md describes the result;
# the DevKit `package` command (tools/devkit/packager.py) drives it.
#
# Install layout (the package's top folder is ${NEOWOLF_PACKAGE_NAME}/):
#   macOS:          NeoWolf.app (game/ in Contents/Resources), README.txt, LICENSE.md, docs/, LICENSES/
#   Windows, Linux: neo_wolf[.exe], game/, README.txt, LICENSE.md, docs/, LICENSES/
# The test executable and the autoplay bot are never installed.

set(NEOWOLF_DISPLAY_NAME "Neo-Wolf")
# Reverse-DNS id derived from the GitHub repository (github.com/Mizan-muf), so it needs no domain.
# Changing it makes macOS treat the app as a different one (the saves are unaffected: they live
# under SDL_GetPrefPath("NeoWolf", "NeoWolf")).
set(NEOWOLF_BUNDLE_ID "io.github.mizan-muf.neowolf")
set(NEOWOLF_REPOSITORY_URL "https://github.com/Mizan-muf/Neo-Wolf-v1")
# The "Required Notice" line of LICENSE.md (PolyForm Noncommercial 1.0.0): Info.plist and the
# Windows version resource carry it too.
set(NEOWOLF_COPYRIGHT "Copyright (c) 2026 Mizan-muf")
set(NEOWOLF_APP_NAME "NeoWolf")  # NeoWolf.app, Contents/MacOS/NeoWolf, Contents/Resources/NeoWolf.icns
set(NEOWOLF_PACKAGE_FILES_DIR "${CMAKE_BINARY_DIR}/package-files")

option(NEOWOLF_MACOS_APP "macOS: also build the NeoWolf.app bundle (the packaged game)" ON)

# `cmake --install` without --prefix goes to build/<preset>/install, never to /usr/local
# (the layout above is a portable folder, not a Unix install).
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install prefix (a staging folder)" FORCE)
endif()

# --- Package name: neo-wolf-<version>-<platform> ----------------------------------------------
if(APPLE)
    set(neowolf_archs "${CMAKE_OSX_ARCHITECTURES}")
    if(NOT neowolf_archs)
        set(neowolf_archs "${CMAKE_HOST_SYSTEM_PROCESSOR}")
    endif()
    if("arm64" IN_LIST neowolf_archs AND "x86_64" IN_LIST neowolf_archs)
        set(NEOWOLF_PACKAGE_PLATFORM "macos-universal")
    else()
        list(JOIN neowolf_archs "-" neowolf_arch_label)
        set(NEOWOLF_PACKAGE_PLATFORM "macos-${neowolf_arch_label}")
    endif()
    set(NEOWOLF_PACKAGE_OS "macos")
    set(NEOWOLF_PACKAGE_FORMAT "zip")
elseif(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$" OR CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
        set(neowolf_arch_label "arm64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(neowolf_arch_label "x64")
    else()
        set(neowolf_arch_label "x86")
    endif()
    set(NEOWOLF_PACKAGE_PLATFORM "windows-${neowolf_arch_label}")
    if(NOT MSVC)
        # A MinGW build (the cross-compile check) must never be mistaken for the MSVC release.
        string(APPEND NEOWOLF_PACKAGE_PLATFORM "-mingw")
    endif()
    set(NEOWOLF_PACKAGE_OS "windows")
    set(NEOWOLF_PACKAGE_FORMAT "zip")
else()
    string(TOLOWER "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}" NEOWOLF_PACKAGE_PLATFORM)
    set(NEOWOLF_PACKAGE_OS "linux")
    set(NEOWOLF_PACKAGE_FORMAT "tar.gz")
endif()
set(NEOWOLF_PACKAGE_NAME "neo-wolf-${PROJECT_VERSION}-${NEOWOLF_PACKAGE_PLATFORM}")

# --- Generated files: icon and README.txt (need Python 3 at build time) -----------------------
set(NEOWOLF_ICNS "")
set(NEOWOLF_ICO "")
set(NEOWOLF_PACKAGE_README "")
if(Python3_Interpreter_FOUND)
    set(neowolf_gen_icon "${PROJECT_SOURCE_DIR}/tools/devkit/gen_icon.py")
    if(APPLE AND NEOWOLF_MACOS_APP)
        find_program(NEOWOLF_ICONUTIL iconutil)
        if(NEOWOLF_ICONUTIL)
            set(NEOWOLF_ICNS "${NEOWOLF_PACKAGE_FILES_DIR}/${NEOWOLF_APP_NAME}.icns")
            set(neowolf_iconset "${NEOWOLF_PACKAGE_FILES_DIR}/${NEOWOLF_APP_NAME}.iconset")
            add_custom_command(OUTPUT "${NEOWOLF_ICNS}"
                COMMAND "${Python3_EXECUTABLE}" "${neowolf_gen_icon}" --iconset "${neowolf_iconset}"
                COMMAND "${NEOWOLF_ICONUTIL}" -c icns "${neowolf_iconset}" -o "${NEOWOLF_ICNS}"
                DEPENDS "${neowolf_gen_icon}"
                COMMENT "Generating the app icon (${NEOWOLF_APP_NAME}.icns)"
                VERBATIM)
        else()
            message(WARNING "iconutil not found: NeoWolf.app is built without an icon.")
        endif()
    elseif(WIN32)
        set(NEOWOLF_ICO "${NEOWOLF_PACKAGE_FILES_DIR}/neowolf.ico")
        add_custom_command(OUTPUT "${NEOWOLF_ICO}"
            COMMAND "${Python3_EXECUTABLE}" "${neowolf_gen_icon}" --ico "${NEOWOLF_ICO}"
            DEPENDS "${neowolf_gen_icon}"
            COMMENT "Generating the application icon (neowolf.ico)"
            VERBATIM)
        add_custom_target(neowolf_icon DEPENDS "${NEOWOLF_ICO}")
    endif()

    set(neowolf_readme_tool "${PROJECT_SOURCE_DIR}/tools/devkit/release_readme.py")
    set(NEOWOLF_PACKAGE_README "${NEOWOLF_PACKAGE_FILES_DIR}/README.txt")
    add_custom_command(OUTPUT "${NEOWOLF_PACKAGE_README}"
        COMMAND "${Python3_EXECUTABLE}" "${neowolf_readme_tool}"
                --platform "${NEOWOLF_PACKAGE_OS}" --version "${PROJECT_VERSION}"
                --sdl-version "${NEOWOLF_SDL2_LINKED_VERSION}" --json-version "${NEOWOLF_JSON_VERSION}"
                --controls "${PROJECT_SOURCE_DIR}/docs/CONTROLS.md" --out "${NEOWOLF_PACKAGE_README}"
        DEPENDS "${neowolf_readme_tool}" "${PROJECT_SOURCE_DIR}/docs/CONTROLS.md"
        COMMENT "Generating the package README.txt (${NEOWOLF_PACKAGE_OS})"
        VERBATIM)
    add_custom_target(neowolf_package_readme ALL DEPENDS "${NEOWOLF_PACKAGE_README}")
else()
    message(WARNING "Python 3 not found: the application icon and the package README.txt are not "
                    "generated, so the install tree is incomplete (the DevKit `package` command fails on it).")
endif()

# --- Windows: icon and version resource in neo_wolf.exe ----------------------------------------
if(WIN32)
    enable_language(RC)
    if(NEOWOLF_ICO)
        # ID 1 = the first icon group: Explorer shows it, and SDL uses it for the window and taskbar.
        set(NEOWOLF_RC_ICON_LINE "1 ICON \"${NEOWOLF_ICO}\"")
    else()
        set(NEOWOLF_RC_ICON_LINE "// No icon: Python 3 was not found when CMake configured this build.")
    endif()
    set(neowolf_rc "${NEOWOLF_PACKAGE_FILES_DIR}/neo_wolf.rc")
    configure_file("${PROJECT_SOURCE_DIR}/packaging/windows/neo_wolf.rc.in" "${neowolf_rc}" @ONLY)
    target_sources(neo_wolf PRIVATE "${neowolf_rc}")
    if(NEOWOLF_ICO)
        set_source_files_properties("${neowolf_rc}" PROPERTIES OBJECT_DEPENDS "${NEOWOLF_ICO}")
        add_dependencies(neo_wolf neowolf_icon)
    endif()
    if(MINGW)
        # The MinGW cross-compile check: link libstdc++, libgcc and winpthread statically, so the
        # exe runs without MinGW's DLLs (MSVC's static CRT is set at the top of CMakeLists.txt).
        target_link_options(neo_wolf PRIVATE -static)
    endif()
endif()

# --- macOS: NeoWolf.app ----------------------------------------------------------------------
# A second executable target built from the same main.cpp: neo_wolf stays the plain developer
# executable (it finds the repository's game/ folder and every test runs it). The bundle in the
# build tree has no content; `cmake --install` adds game/ to Contents/Resources and signs it.
if(APPLE AND NEOWOLF_MACOS_APP)
    if(NEOWOLF_ICNS)
        set(NEOWOLF_APP_ICON_FILE "${NEOWOLF_APP_NAME}.icns")
    else()
        set(NEOWOLF_APP_ICON_FILE "")
    endif()
    set(NEOWOLF_APP_EXECUTABLE "${NEOWOLF_APP_NAME}")
    set(neowolf_plist "${NEOWOLF_PACKAGE_FILES_DIR}/Info.plist")
    configure_file("${PROJECT_SOURCE_DIR}/packaging/macos/Info.plist.in" "${neowolf_plist}" @ONLY)

    add_executable(neo_wolf_app MACOSX_BUNDLE src/main.cpp)
    target_link_libraries(neo_wolf_app PRIVATE neowolf_engine neowolf_warnings)
    set_target_properties(neo_wolf_app PROPERTIES
        OUTPUT_NAME "${NEOWOLF_APP_NAME}"
        MACOSX_BUNDLE_INFO_PLIST "${neowolf_plist}")
    if(NEOWOLF_ICNS)
        target_sources(neo_wolf_app PRIVATE "${NEOWOLF_ICNS}")
        set_source_files_properties("${NEOWOLF_ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    endif()
endif()

# --- Install rules ---------------------------------------------------------------------------
if(APPLE AND NEOWOLF_MACOS_APP)
    set(NEOWOLF_PACKAGE_EXECUTABLE "${NEOWOLF_APP_NAME}.app/Contents/MacOS/${NEOWOLF_APP_NAME}")
    set(NEOWOLF_PACKAGE_CONTENT_ROOT "${NEOWOLF_APP_NAME}.app/Contents/Resources")
    install(TARGETS neo_wolf_app BUNDLE DESTINATION .)
else()
    set(NEOWOLF_PACKAGE_EXECUTABLE "neo_wolf${CMAKE_EXECUTABLE_SUFFIX}")
    set(NEOWOLF_PACKAGE_CONTENT_ROOT ".")
    install(TARGETS neo_wolf RUNTIME DESTINATION .)
endif()
# The game's content. The entity CSV files are an optional, lossy export (the game loads the
# JSON files named in campaign.json), so they are not shipped.
install(DIRECTORY "${PROJECT_SOURCE_DIR}/game" DESTINATION "${NEOWOLF_PACKAGE_CONTENT_ROOT}"
        PATTERN "*.entities.csv" EXCLUDE
        PATTERN ".DS_Store" EXCLUDE)
if(NEOWOLF_PACKAGE_README)
    install(FILES "${NEOWOLF_PACKAGE_README}" DESTINATION .)
endif()
# The project's licence (PolyForm Noncommercial 1.0.0): its Notices clause requires the terms and
# the "Required Notice:" line to travel with every copy.
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE.md" DESTINATION .)
install(FILES "${PROJECT_SOURCE_DIR}/docs/CONTROLS.md" "${PROJECT_SOURCE_DIR}/docs/TROUBLESHOOTING.md"
        DESTINATION docs)
# Third-party notices (docs/THIRD_PARTY_LICENSES.md): the licence texts as shipped upstream.
install(FILES "${PROJECT_SOURCE_DIR}/docs/THIRD_PARTY_LICENSES.md" DESTINATION LICENSES)
install(FILES "${PROJECT_SOURCE_DIR}/third_party/nlohmann/LICENSE.MIT"
        DESTINATION LICENSES RENAME "nlohmann-json-LICENSE.MIT.txt")
if(NEOWOLF_SDL2_LICENSE_FILE)
    install(FILES "${NEOWOLF_SDL2_LICENSE_FILE}" DESTINATION LICENSES RENAME "SDL2-LICENSE.txt")
else()
    message(WARNING "SDL2's LICENSE.txt was not found (system SDL2?): the install tree lacks LICENSES/SDL2-LICENSE.txt.")
endif()

if(APPLE AND NEOWOLF_MACOS_APP)
    # Ad-hoc signature, after everything inside the bundle (Contents/Resources/game) is in place:
    # it seals the resources, and Apple silicon needs a valid signature to run the arm64 slice.
    # Not notarized (no Developer ID): see docs/RELEASE.md. Runs for `cmake --install` and cpack.
    install(CODE "set(neowolf_app_name \"${NEOWOLF_APP_NAME}.app\")")
    install(CODE [[
        set(neowolf_app "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${neowolf_app_name}")
        message(STATUS "Signing (ad hoc): ${neowolf_app}")
        execute_process(COMMAND codesign --force --deep -s - "${neowolf_app}"
                        RESULT_VARIABLE neowolf_codesign_result)
        if(NOT neowolf_codesign_result EQUAL 0)
            message(FATAL_ERROR "codesign failed (${neowolf_codesign_result}) for ${neowolf_app}")
        endif()
    ]])
endif()

# --- What the DevKit packager needs to know (read by tools/devkit/packager.py) -----------------
if(NEOWOLF_MULTI_CONFIG)
    set(neowolf_multi_config_json "true")
else()
    set(neowolf_multi_config_json "false")
endif()
file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/neowolf-package.json" CONTENT [[
{
  "schema": 1,
  "version": "@PROJECT_VERSION@",
  "os": "@NEOWOLF_PACKAGE_OS@",
  "platform": "@NEOWOLF_PACKAGE_PLATFORM@",
  "package": "@NEOWOLF_PACKAGE_NAME@",
  "format": "@NEOWOLF_PACKAGE_FORMAT@",
  "executable": "@NEOWOLF_PACKAGE_EXECUTABLE@",
  "content_root": "@NEOWOLF_PACKAGE_CONTENT_ROOT@",
  "compiler": "@CMAKE_CXX_COMPILER_ID@",
  "multi_config": @neowolf_multi_config_json@
}
]] @ONLY)

# --- CPack (an alternative to the DevKit: `cpack --config build/<preset>/CPackConfig.cmake`) ---
set(CPACK_PACKAGE_NAME "neo-wolf")
set(CPACK_PACKAGE_VENDOR "Neo-Wolf project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Neo-Wolf, a software raycaster game")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${NEOWOLF_PACKAGE_NAME}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/cpack")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
if(NEOWOLF_PACKAGE_FORMAT STREQUAL "zip")
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()
set(CPACK_SOURCE_GENERATOR "")
include(CPack)
