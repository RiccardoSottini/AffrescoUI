#pragma once

/// @file
/// @brief The `search` provider's factory (SearchProvider.cpp), beside the others' in Providers.hpp.

#include "affresco/services/Provider.hpp"

#include <memory>

namespace affresco::services {

/// @brief The `search` provider: a granted folder's text files searched (streamed, one event per file) and a file's
/// matches replaced, the new text returned for an `fs` write.
[[nodiscard]] std::unique_ptr<Provider> makeSearchProvider();

} // namespace affresco::services
