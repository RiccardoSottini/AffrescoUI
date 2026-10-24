# audio: an output device that mixes voices playing files, bytes in memory, streams pushed from any thread and
# generators (README.md here). Uses core (the Executor its events go to). Backend: miniaudio
# (cmake/AffrescoUIAudio.cmake): its devices, decoders and data converter.
affrescoui_module(audio
	src/Engine.cpp src/Voice.cpp src/Sources.cpp src/Buffers.cpp src/Decoders.cpp
	src/Miniaudio.c)
set_source_files_properties(src/Miniaudio.c PROPERTIES COMPILE_OPTIONS $<IF:$<C_COMPILER_ID:MSVC>,/w,-w> SKIP_LINTING ON)
target_include_directories(affrescoui_audio SYSTEM PRIVATE ${miniaudio_SOURCE_DIR})
target_compile_definitions(affrescoui_audio PRIVATE ${AFFRESCOUI_MINIAUDIO_DEFINITIONS})
target_link_libraries(affrescoui_audio PUBLIC affrescoui::core PRIVATE ${AFFRESCOUI_MINIAUDIO_LIBRARIES})
