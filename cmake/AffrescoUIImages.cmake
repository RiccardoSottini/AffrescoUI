# The image decoder of the render module, included by modules/render/CMakeLists.txt: stb_image 2.30 decodes PNG and
# JPEG. One header, downloaded at a pinned commit and checked by its hash, like the other dependencies; compiled into
# the render library (src/Bitmap.cpp), with only PNG and JPEG and no file access of its own.
set(AFFRESCOUI_STB_IMAGE_COMMIT 013ac3beddff3dbffafd5177e7972067cd2b5083) # stb_image 2.30
FetchContent_Declare(stb_image
	URL https://raw.githubusercontent.com/nothings/stb/${AFFRESCOUI_STB_IMAGE_COMMIT}/stb_image.h
	URL_HASH SHA256=594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3
	DOWNLOAD_NO_EXTRACT TRUE)
FetchContent_MakeAvailable(stb_image)
