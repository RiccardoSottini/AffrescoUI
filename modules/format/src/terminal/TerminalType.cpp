/// @file
/// @brief The `terminal` node (TerminalView.hpp): a VT terminal showing a session of the engine's terminals
/// (TerminalSessions.hpp), whose program runs on a service of the `pty` provider.

#include "TerminalView.hpp"
#include "TypeRegistry.hpp"

#include <memory>
#include <string>

namespace affresco::format {

namespace {

/// @brief Applies a terminal's properties.
/// @param panel The panel, a TerminalView.
/// @param values Its evaluated properties.
/// @param reader Reads the colours and the font, and has the engine's terminals.
/// @param path Its place, for errors.
void applyTerminal(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	if(auto* view = dynamic_cast<TerminalView*>(&panel)) view->apply(reader.terminals(), values, reader, path);
}

} // namespace

void registerTerminalType(TypeRegistry& registry) {
	const auto set = std::make_shared<PropertySet>();
	set->properties = {{"session", ValueKind::Text},      {"service", ValueKind::Text},
	                   {"font", ValueKind::Font},         {"textColor", ValueKind::Color},
	                   {"cursorColor", ValueKind::Color}, {"scrollback", ValueKind::Number},
	                   {"allowTitle", ValueKind::Boolean}};
	set->apply = applyTerminal;
	set->everyUpdate = true; // colours follow the theme
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const TerminalView*>(&panel) != nullptr; };
	registry.add({"terminal",
	              "A VT terminal showing a session, its program on a \"pty\" service; runInTerminal types lines into it",
	              [] { return std::make_unique<TerminalView>(); }, set, false});
}

} // namespace affresco::format
