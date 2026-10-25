# audio: an output device that mixes voices playing files, bytes in memory, streams pushed from any thread and
# generators (README.md here). Uses core (the Executor its events go to). Backend: miniaudio
# (cmake/AffrescoUIAudio.cmake): its devices, decoders and data converter.
affrescoui_module(audio
	src/Engine.cpp src/Voice.cpp src/Sources.cpp src/Buffers.cpp src/Decoders.cpp src/Sound.cpp src/Spectrum.cpp
	src/Miniaudio.c)
# -fwrapv: its decoders' arithmetic on untrusted samples may overflow a signed integer (MS-ADPCM does, found by
# tests/fuzz/audio.cpp); wrapping makes that defined, as MSVC's code already does
set_source_files_properties(src/Miniaudio.c PROPERTIES COMPILE_OPTIONS "$<IF:$<C_COMPILER_ID:MSVC>,/w,-w;-fwrapv>"
	SKIP_LINTING ON)
target_include_directories(affrescoui_audio SYSTEM PRIVATE ${miniaudio_SOURCE_DIR})
target_compile_definitions(affrescoui_audio PRIVATE ${AFFRESCOUI_MINIAUDIO_DEFINITIONS})
target_link_libraries(affrescoui_audio PUBLIC affrescoui::core PRIVATE nlohmann_json::nlohmann_json
	${AFFRESCOUI_MINIAUDIO_LIBRARIES}) # nlohmann_json: sound descriptions (Sound.cpp)
