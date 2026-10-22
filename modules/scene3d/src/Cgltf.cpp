/// @file
/// @brief cgltf's implementation, compiled once (cmake/AffrescoUIGltf.cmake). Files are never opened by cgltf: the
/// loader reads them itself (GltfLoad.cpp), so only its parser, validator and accessor readers are used.

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
