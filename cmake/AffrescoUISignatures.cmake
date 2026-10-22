# Monocypher, the Ed25519 signatures of feature packages and catalogues (RFC 0004, decision D12), included by
# CMakeLists.txt. Downloaded as its release archive, pinned by its hash like the other dependencies; its two C files
# (the library and its optional Ed25519, which uses SHA-512) are compiled into the format module, which is the only one
# that checks signatures, so nothing new is installed or exported.

set(AFFRESCOUI_MONOCYPHER_VERSION 4.0.2)
FetchContent_Declare(monocypher
	URL https://monocypher.org/download/monocypher-${AFFRESCOUI_MONOCYPHER_VERSION}.tar.gz
	URL_HASH SHA256=38d07179738c0c90677dba3ceb7a7b8496bcfea758ba1a53e803fed30ae0879c
	SOURCE_SUBDIR affrescoui-builds-its-two-files SYSTEM) # a folder it doesn't have: its makefile isn't used
FetchContent_MakeAvailable(monocypher)
set(AFFRESCOUI_MONOCYPHER_DIR ${monocypher_SOURCE_DIR}/src)
set(AFFRESCOUI_MONOCYPHER_SOURCES ${AFFRESCOUI_MONOCYPHER_DIR}/monocypher.c
	${AFFRESCOUI_MONOCYPHER_DIR}/optional/monocypher-ed25519.c)
# Its warnings are not ours to fix
set_source_files_properties(${AFFRESCOUI_MONOCYPHER_SOURCES} PROPERTIES COMPILE_OPTIONS $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>
	SKIP_LINTING ON)
