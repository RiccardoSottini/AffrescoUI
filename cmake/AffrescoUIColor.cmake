# Little CMS 2 (lcms2), the colour management of the imaging module: ICC profiles, conversions to the display, soft
# proofing. Included by modules/imaging/CMakeLists.txt. Downloaded as its release archive, pinned by its hash like the
# other dependencies; only its core library (src/, MIT) is compiled, into the imaging module: its plugins folder, under
# the GPL, is never built. Profiles are parsed from memory, under imaging::Limits::profileBytes.

set(AFFRESCOUI_LCMS2_VERSION 2.16)
FetchContent_Declare(lcms2
	URL https://github.com/mm2/Little-CMS/releases/download/lcms${AFFRESCOUI_LCMS2_VERSION}/lcms2-${AFFRESCOUI_LCMS2_VERSION}.tar.gz
	URL_HASH SHA256=d873d34ad8b9b4cea010631f1a6228d2087475e4dc5e763eb81acc23d9d45a51
	SOURCE_SUBDIR affrescoui-builds-its-sources SYSTEM) # a folder it doesn't have: its build files aren't used
FetchContent_MakeAvailable(lcms2)
set(AFFRESCOUI_LCMS2_INCLUDE_DIR ${lcms2_SOURCE_DIR}/include)
set(AFFRESCOUI_LCMS2_SOURCES)
foreach(file cmsalpha cmscam02 cmscgats cmscnvrt cmserr cmsgamma cmsgmt cmshalf cmsintrp cmsio0 cmsio1 cmslut cmsmd5
	cmsmtrx cmsnamed cmsopt cmspack cmspcs cmsplugin cmsps2 cmssamp cmssm cmstypes cmsvirt cmswtpnt cmsxform)
	list(APPEND AFFRESCOUI_LCMS2_SOURCES ${lcms2_SOURCE_DIR}/src/${file}.c)
endforeach()
# Its warnings are not ours to fix
set_source_files_properties(${AFFRESCOUI_LCMS2_SOURCES} PROPERTIES COMPILE_OPTIONS $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>
	SKIP_LINTING ON)
