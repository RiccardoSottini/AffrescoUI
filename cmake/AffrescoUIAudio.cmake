# The audio module's backend, included by CMakeLists.txt: miniaudio 0.11.25 (public domain or MIT No Attribution) plays
# to the system's output (CoreAudio, WASAPI, PulseAudio or ALSA, those of Linux loaded at run time) and decodes WAV,
# FLAC and MP3. One header, downloaded at a pinned commit and checked by its hash like the other dependencies; compiled
# once (modules/audio/src/Miniaudio.c) without the strict warnings. It is built without its encoders, generators,
# resource manager, node graph and high-level engine: the module mixes its own voices. AFFRESCOUI_AUDIO=OFF leaves
# the module out: screens' "audio" steps then say the build has no audio. The browser build leaves it out too (it has
# no threads, which the event thread and the stream decoder need).
option(AFFRESCOUI_AUDIO "Build the audio module (modules/audio) and play screens' sounds, through miniaudio" ON)
if(EMSCRIPTEN)
	set(AFFRESCOUI_AUDIO OFF CACHE BOOL "" FORCE)
endif()
if(NOT AFFRESCOUI_AUDIO)
	return()
endif()

set(AFFRESCOUI_MINIAUDIO_COMMIT 9634bedb5b5a2ca38c1ee7108a9358a4e233f14d) # miniaudio 0.11.25
FetchContent_Declare(miniaudio
	URL https://raw.githubusercontent.com/mackron/miniaudio/${AFFRESCOUI_MINIAUDIO_COMMIT}/miniaudio.h
	URL_HASH SHA256=ac7af4de748b7e26b777f37e01cee313a308a7296a3eb080e2906b320cc55c89
	DOWNLOAD_NO_EXTRACT TRUE)
FetchContent_MakeAvailable(miniaudio)

# What miniaudio is built with, for its C file and the module's C++ ones alike (its structures depend on them)
set(AFFRESCOUI_MINIAUDIO_DEFINITIONS MA_NO_ENCODING MA_NO_GENERATION MA_NO_RESOURCE_MANAGER MA_NO_NODE_GRAPH
	MA_NO_ENGINE)
if(APPLE)
	# Linked, not loaded at run time: the frameworks are part of every macOS
	list(APPEND AFFRESCOUI_MINIAUDIO_DEFINITIONS MA_NO_RUNTIME_LINKING)
	set(AFFRESCOUI_MINIAUDIO_LIBRARIES "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
elseif(WIN32)
	set(AFFRESCOUI_MINIAUDIO_LIBRARIES "") # WASAPI's libraries are loaded at run time
else()
	find_package(Threads REQUIRED)
	set(AFFRESCOUI_MINIAUDIO_LIBRARIES Threads::Threads ${CMAKE_DL_LIBS} m) # PulseAudio and ALSA are loaded at run time
endif()
