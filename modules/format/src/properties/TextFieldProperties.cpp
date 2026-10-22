/// @file
/// @brief The form properties of text inputs: "secret" (a password, shown as dots), "maxLength" and "accept" (which
/// characters typing and pasting may add).

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/CodeInput.hpp"
#include "affresco/render/TextArea.hpp"

#include <cstddef>
#include <memory>
#include <string>

namespace affresco::format {

namespace {

/// The most characters "maxLength" may ask for.
constexpr int longestField = 100000;

/// @brief Reads "accept".
/// @param value "any", "digits", "letters" or "alphanumeric".
/// @param at Its place, for errors.
/// @return The characters a text input takes.
/// @throws DocumentError If it is none of them.
TextArea::Accept acceptFrom(const json& value, const std::string& at) {
	const std::string name = string(value, at);
	if(name == "any") return TextArea::Accept::Any;
	if(name == "digits") return TextArea::Accept::Digits;
	if(name == "letters") return TextArea::Accept::Letters;
	if(name == "alphanumeric") return TextArea::Accept::Alphanumeric;
	fail(at, "expected any, digits, letters or alphanumeric");
}

/// @brief Sets a text input's form properties: those the node gives, the defaults for the others.
/// @param panel The panel, a TextArea.
/// @param values Its "secret", "maxLength" and "accept", if given.
/// @param reader Not used: none of them is a colour or a font.
/// @param path The node's place, for errors.
void applyTextField(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	static_cast<void>(reader);
	auto* area = dynamic_cast<TextArea*>(&panel);
	if(!area) return;
	area->setSecret(values.contains("secret") && boolean(values["secret"], path + ".secret"));
	if(dynamic_cast<CodeInput*>(area)) return; // its length and digits are its own
	int most = 0;
	if(values.contains("maxLength")) {
		most = integer(values["maxLength"], path + ".maxLength");
		if(most < 0 || most > longestField) fail(path + ".maxLength", "expected 0 (no limit) to 100000");
	}
	area->setMaxLength(static_cast<std::size_t>(most));
	area->setAccept(values.contains("accept") ? acceptFrom(values["accept"], path + ".accept") : TextArea::Accept::Any);
}

} // namespace

void registerTextFieldProperties(TypeRegistry& registry) {
	auto set = std::make_shared<PropertySet>();
	set->properties = {{"secret", ValueKind::Boolean}, {"maxLength", ValueKind::Number}, {"accept", ValueKind::Text}};
	set->apply = applyTextField;
	set->everyUpdate = true; // each setter is cheap, and a key the node drops goes back to its default
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const TextArea*>(&panel) != nullptr; };
	registry.addProperties(std::move(set));
}

} // namespace affresco::format
