/// @file
/// @brief channel::Session's "convert" message: a value written in another notation (the notation module), so that a
/// peer can read and write documents in the Lisp notation while the protocol and the history stay JSON.

#include "affresco/channel/Session.hpp"
#include "affresco/notation/Notation.hpp"

#include <optional>
#include <string>

namespace affresco::channel {

// convert: text in a notation into its value, or a value into its text; it reads nothing of the document
bool Session::handleConvert(const Incoming& in) {
	if(in.type != "convert") return false;
	const json& m = in.message;
	const std::string name = expect::string(m.value("notation", json()), "convert.notation");
	const std::optional<notation::Notation> spelling = notation::notationNamed(name);
	if(!spelling) expect::fail("convert.notation", R"(expected "json" or "lisp")");
	if(m.contains("text") == m.contains("value"))
		expect::fail("convert", R"(expected either "text", to read, or "value", to write)");
	json reply = {{"type", "converted"}, {"notation", name}};
	if(m.contains("text")) reply["value"] = notation::read(expect::string(m["text"], "convert.text"), *spelling);
	else reply["text"] = notation::write(m["value"], *spelling);
	reply["id"] = in.id;
	send(reply);
	return true;
}

} // namespace affresco::channel
