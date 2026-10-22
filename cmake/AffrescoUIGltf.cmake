# glTF 2.0 for the scene3d module, included by modules/scene3d/CMakeLists.txt: cgltf 1.15 (MIT) parses and validates
# the files. One header, downloaded at a pinned commit and checked by its hash, like the other dependencies; compiled
# once (src/Cgltf.cpp) without the strict warnings, and never asked to open a file or a URI itself.
set(AFFRESCOUI_CGLTF_COMMIT 360db1a95480fe102ae9c69b27c5d101167ff5ba) # cgltf 1.15
FetchContent_Declare(cgltf
	URL https://raw.githubusercontent.com/jkuhlmann/cgltf/${AFFRESCOUI_CGLTF_COMMIT}/cgltf.h
	URL_HASH SHA256=e378a21c084bf1f288bb799de827bb26906efb024255f1ecf1705ea13f11c6ec
	DOWNLOAD_NO_EXTRACT TRUE)
FetchContent_MakeAvailable(cgltf)
