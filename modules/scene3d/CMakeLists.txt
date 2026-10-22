# scene3d: general 3D (README.md here): maths, a scene of nodes, meshes, materials, lights and cameras, bounds and
# culling, glTF 2.0 through cgltf (cmake/AffrescoUIGltf.cmake), the orbit camera, picking and gizmo maths, and a PBR
# forward renderer that a screen shows as a GpuSurface. Vertices and pixels stay in an Assets store, never in JSON.
include(${PROJECT_SOURCE_DIR}/cmake/AffrescoUIGltf.cmake)
affrescoui_module(scene3d
	src/Math.cpp
	src/Bounds.cpp
	src/Mesh.cpp
	src/Primitives.cpp
	src/Scene.cpp
	src/SceneJson.cpp
	src/Animation.cpp
	src/GltfLoad.cpp
	src/GltfConvert.cpp
	src/GltfExport.cpp
	src/Cgltf.cpp)
set_source_files_properties(src/Cgltf.cpp PROPERTIES COMPILE_OPTIONS $<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w> SKIP_LINTING ON)
target_include_directories(affrescoui_scene3d SYSTEM PRIVATE ${cgltf_SOURCE_DIR})
target_link_libraries(affrescoui_scene3d PUBLIC affrescoui::render affrescoui::core nlohmann_json::nlohmann_json)
