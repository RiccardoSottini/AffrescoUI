/// @file
/// @brief Packages' format (README.md here): a package read and checked, written back, and the tree of a source
/// document, which a package names its base by.

#include "affresco/format/Package.hpp"

#include "FeatureSignatures.hpp" // canonicalJson(): the tree is the hash of a document's canonical form

#include "affresco/core/Sha256.hpp"

namespace affresco::format {

std::string documentTree(const json& sourceDocument) { return "sha256:" + sha256::hex(canonicalJson(sourceDocument)); }

} // namespace affresco::format
