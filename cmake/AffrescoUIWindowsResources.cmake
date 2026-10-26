# The Windows programs' resources: the version information Explorer shows under Properties > Details, and that code
# signing checks (the product name and version a signed program must carry, RELEASING.md), and the program's icon.
#
#   affrescoui_windows_resources(<target> <file name> <description> [ICON <file.ico>])
#
# Does nothing on other systems.
function(affrescoui_windows_resources target file description)
	if(NOT WIN32)
		return()
	endif()
	cmake_parse_arguments(PARSE_ARGV 3 resources "" "ICON" "")
	set(icon "")
	if(resources_ICON)
		set(icon "1 ICON \"${resources_ICON}\"\n")
	endif()
	set(numbers "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
	file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${target}.rc" CONTENT [=[
#include <winver.h>
@icon@
VS_VERSION_INFO VERSIONINFO
FILEVERSION @numbers@
PRODUCTVERSION @numbers@
FILEFLAGSMASK VS_FFI_FILEFLAGSMASK
FILEFLAGS 0
FILEOS VOS_NT_WINDOWS32
FILETYPE VFT_APP
FILESUBTYPE VFT2_UNKNOWN
BEGIN
	BLOCK "StringFileInfo"
	BEGIN
		BLOCK "040904B0"
		BEGIN
			VALUE "FileDescription", "@description@"
			VALUE "FileVersion", "@PROJECT_VERSION@"
			VALUE "InternalName", "@target@"
			VALUE "LegalCopyright", "Copyright 2017-2026 Riccardo Sottini. Apache License 2.0."
			VALUE "OriginalFilename", "@file@"
			VALUE "ProductName", "AffrescoUI"
			VALUE "ProductVersion", "@PROJECT_VERSION@"
		END
	END
	BLOCK "VarFileInfo"
	BEGIN
		VALUE "Translation", 0x409, 1200
	END
END
]=] @ONLY)
	target_sources(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${target}.rc")
endfunction()
