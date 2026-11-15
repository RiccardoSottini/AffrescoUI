/// @file
/// @brief makeEditorPeer() (EditorLink.hpp): the protocol's side of a connection. A stub until the protocol's package
/// (f2c) fills it: its peer answers nothing, and nothing calls it yet.

#include "EditorLink.hpp"

namespace affresco::net {

namespace {

/// @brief A peer that answers nothing.
class SilentPeer final : public EditorPeer {
public:
	/// @brief Drops a message. @param message It.
	void receive(const bencode::Value& message) override { static_cast<void>(message); }
	/// @brief Nothing to let go of.
	void closed() override {}
};

} // namespace

std::unique_ptr<EditorPeer> makeEditorPeer(EditorLink& /*link*/, EditorHost& /*host*/,
                                           const EditorServerOptions& /*options*/, EditorShared& /*shared*/) {
	return std::make_unique<SilentPeer>();
}

} // namespace affresco::net
