# vcpkg overlay port for AffrescoUI, built from the release's source tarball. A template: tools/release/packaging.py
# fills in the version and the SHA-512 (packaging/README.md). Try it with:
#   vcpkg install affrescoui --overlay-ports=<filled copy>/vcpkg
#
# The library installs the exact nlohmann/json, GLFW and FreeType it was built with, downloaded by its own CMake and
# checked against their SHA-256 (cmake/dependencies.json); AffrescoUIConfig.cmake finds them in this prefix. Only
# static libraries are supported (cmake/AffrescoUIInstall.cmake says why), and one configuration, Release.
set(VCPKG_BUILD_TYPE release)
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_download_distfile(ARCHIVE
	URLS "https://github.com/RiccardoSottini/AffrescoUI/releases/download/v${VERSION}/affrescoui-${VERSION}-source.tar.gz"
	FILENAME "affrescoui-${VERSION}-source.tar.gz"
	SHA512 @SOURCE_SHA512@)
vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}")

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}"
	OPTIONS
		-DAFFRESCOUI_INSTALL=ON
		-DAFFRESCOUI_BUILD_EXAMPLES=OFF
		-DAFFRESCOUI_BUILD_TESTS=OFF
		-DAFFRESCOUI_BUILD_TOOLS=OFF)
vcpkg_cmake_install()

# Each CMake package to share/<name>, where find_package looks in a vcpkg tree
vcpkg_cmake_config_fixup(PACKAGE_NAME affrescoui CONFIG_PATH lib/cmake/AffrescoUI DO_NOT_DELETE_PARENT_CONFIG_PATH)
vcpkg_cmake_config_fixup(PACKAGE_NAME glfw3 CONFIG_PATH lib/cmake/glfw3 DO_NOT_DELETE_PARENT_CONFIG_PATH)
vcpkg_cmake_config_fixup(PACKAGE_NAME freetype CONFIG_PATH lib/cmake/freetype DO_NOT_DELETE_PARENT_CONFIG_PATH)
if(EXISTS "${CURRENT_PACKAGES_DIR}/lib/cmake/CURL")
	vcpkg_cmake_config_fixup(PACKAGE_NAME CURL CONFIG_PATH lib/cmake/CURL DO_NOT_DELETE_PARENT_CONFIG_PATH)
endif()
vcpkg_cmake_config_fixup(PACKAGE_NAME nlohmann_json CONFIG_PATH share/cmake/nlohmann_json)
vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib/cmake" "${CURRENT_PACKAGES_DIR}/share/doc")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE" "${SOURCE_PATH}/NOTICE"
	"${SOURCE_PATH}/THIRD_PARTY_NOTICES.md")
