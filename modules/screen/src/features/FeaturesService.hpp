#pragma once

/// @file
/// @brief The `features` provider (RFC 0004): what the host's catalogues offer, and installing or upgrading a feature
/// from them by revision. Enabling, disabling and removing are revisions on `/features/<name>` a screen submits itself.

#include "affresco/format/Features.hpp"
#include "affresco/services/Provider.hpp"
#include "affresco/vcs/Repository.hpp"

#include <memory>

namespace affresco::detail {

/// @brief Tells whether a Screen still exists, from its guard.
/// @param guard The Screen's guard.
/// @return false once the Screen ends: the flag is read, since other tasks still queued may keep it from expiring.
[[nodiscard]] inline bool screenAlive(const std::weak_ptr<bool>& guard) {
	const std::shared_ptr<bool> flag = guard.lock();
	return flag && *flag;
}

/// @brief Makes the `features` provider of a Screen.
/// @param features The host's catalogues and trusted publishers; outlives the provider.
/// @param repository The Screen's document; its revisions are reviewed as the document's "permissions" say.
/// @param alive The Screen's guard: false once it ends, so a queued submission is dropped.
/// @return The provider.
[[nodiscard]] std::unique_ptr<services::Provider> makeFeaturesProvider(const format::Features& features,
                                                                       vcs::Repository& repository,
                                                                       std::weak_ptr<bool> alive);

} // namespace affresco::detail
