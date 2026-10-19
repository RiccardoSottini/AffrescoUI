# core: the contracts between modules (headers only). JSON is opt-in: the rendering engine doesn't see it.
add_library(affrescoui_core INTERFACE)
add_library(affrescoui::core ALIAS affrescoui_core)
target_include_directories(affrescoui_core INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/modules/core/include>)
target_compile_features(affrescoui_core INTERFACE cxx_std_23)
