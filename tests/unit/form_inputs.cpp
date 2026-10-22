/// @file
/// @brief Form inputs (render's TextArea and CodeInput, format's TextFieldProperties, CodeInputType and
/// ExpressionFunctionsForm): secret fields, maxLength and accept, the one-time-code input, and the passwordStrength()
/// and isEmail() functions.

#include "Expression.hpp"
#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/CodeInput.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen built in a headless window.
struct Built {
	ManualClock clock;                        ///< The window's time.
	std::unique_ptr<affresco::Window> window; ///< The window.
	affresco::Screen screen;                  ///< The screen.

	/// @brief Builds a screen.
	/// @param text The screen.
	explicit Built(const std::string& text)
	    : window(createHeadlessWindow(400, 300, clock)), screen(affresco::Screen::fromString(text)) {
		screen.build(*window);
		window->render();
	}

	/// @brief Types characters into the focused panel.
	/// @param text The characters (ASCII).
	void type(const std::string& text) {
		for(const char c : text) window->routeChar({static_cast<char32_t>(c)});
		window->render();
	}

	/// @brief Reads a state key.
	/// @param key Its name.
	/// @return Its value.
	[[nodiscard]] json state(const char* key) const { return json::parse(screen.stateJson())[key]; }
};

/// @brief A screen with one focused field.
/// @param keys The field's other keys, as JSON members (a leading comma included).
/// @return The screen.
std::string fieldScreen(const std::string& keys) {
	return R"json({ "format": "affrescoui-screen", "version": 2, "state": { "text": "" },
		"root": { "id": "root", "children": [{ "id": "field", "type": "textarea", "bind": "state.text",
		"autofocus": true, "width": 200, "height": 24)json" +
	       keys + "}] } }";
}

/// @brief Evaluates an expression with no state, as a screen would.
/// @param expression The expression, without braces.
/// @return Its value.
json evaluate(const std::string& expression) {
	static const json state = json::object(), window = {{"width", 800}, {"height", 600}}, revision = 1;
	static const json event = json::object(), globals = json::object();
	static const affresco::format::Scope locals{};
	const affresco::format::Context context{state, window, revision, event, locals, globals};
	return affresco::format::evaluate("{{" + expression + "}}", context, "test");
}

} // namespace

TEST_CASE("form: a secret field keeps its text, shows dots and never gives the text to assistive tools") {
	Built built(fieldScreen(R"(, "secret": true)"));
	built.type("hunter2x");
	CHECK(built.state("text") == "hunter2x");
	auto* field = dynamic_cast<affresco::TextArea*>(built.screen.find("field"));
	REQUIRE(field);
	CHECK(field->secret());
	CHECK(affresco::accessibilityTree(*field).value.empty());
}

TEST_CASE("form: maxLength and accept limit what typing adds") {
	Built built(fieldScreen(R"(, "maxLength": 4, "accept": "digits")"));
	built.type("1a2b3c4d5");
	CHECK(built.state("text") == "1234");
}

TEST_CASE("form: a field that drops its keys goes back to the defaults") {
	affresco::TextArea area;
	area.setMaxLength(2);
	area.setAccept(affresco::TextArea::Accept::Letters);
	CHECK_FALSE(area.accepts(U'7'));
	CHECK(area.accepts(U'è')); // a letter beyond ASCII
	CHECK_FALSE(area.accepts(U'×'));
	area.setAccept(affresco::TextArea::Accept::Any);
	CHECK(area.accepts(U'7'));
}

TEST_CASE("form: an invalid maxLength, accept or code length is an error") {
	CHECK_THROWS(Built(fieldScreen(R"(, "maxLength": -1)")));
	CHECK_THROWS(Built(fieldScreen(R"(, "accept": "emoji")")));
	CHECK_THROWS(Built(R"json({ "format": "affrescoui-screen", "version": 2,
		"root": { "id": "root", "children": [{ "id": "code", "type": "codeInput", "length": 13 }] } })json"));
}

TEST_CASE("form: a code input takes digits up to its length and submits nothing more") {
	Built built(R"json({ "format": "affrescoui-screen", "version": 2, "state": { "code": "" },
		"root": { "id": "root", "children": [{ "id": "code", "type": "codeInput", "bind": "state.code",
		"length": 4, "autofocus": true, "width": 200, "height": 40 }] } })json");
	built.type("9x8 76543");
	CHECK(built.state("code") == "9876");
	auto* code = dynamic_cast<affresco::CodeInput*>(built.screen.find("code"));
	REQUIRE(code);
	CHECK(code->length() == 4);
	CHECK(affresco::accessibleRole(*code) == "textbox");
}

TEST_CASE("form: passwordStrength scores length and variety, and says what is missing") {
	CHECK(evaluate("passwordStrength('').label") == "empty");
	CHECK(evaluate("passwordStrength('abc').score") == 0);
	CHECK(evaluate("passwordStrength('abcdefgh').label") == "weak");
	CHECK(evaluate("passwordStrength('abcdefg1A').label") == "fair");
	CHECK(evaluate("passwordStrength('Abcdefgh1234').label") == "good");
	CHECK(evaluate("passwordStrength('Abcdefgh123!').score") == 4);
	CHECK(evaluate("passwordStrength('abcdefgh').missing") == json::array({"length", "upper", "digit", "symbol"}));
}

TEST_CASE("form: isEmail accepts the shape of an address and nothing else") {
	CHECK(evaluate("isEmail('ada@example.org')") == true);
	CHECK(evaluate("isEmail(' ada.l+news@mail.example.org ')") == true);
	CHECK(evaluate("isEmail('ada@example')") == false);
	CHECK(evaluate("isEmail('ada@@example.org')") == false);
	CHECK(evaluate("isEmail('a da@example.org')") == false);
	CHECK(evaluate("isEmail('.ada@example.org')") == false);
	CHECK(evaluate("isEmail('ada@example..org')") == false);
	CHECK(evaluate("isEmail('')") == false);
}

TEST_CASE("form: a comboBox filters its options as one types, and the keys pick one") {
	Built built(R"json({ "format": "affrescoui-screen", "version": 2, "state": { "country": { "value": "" } },
		"root": { "id": "root", "children": [{ "id": "box", "use": "comboBox", "with": { "name": "country",
		"label": "Country", "options": ["Italy", "Ireland", "Spain", "Iceland"] } }] } })json");
	built.window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, {20.0f, 15.0f}});
	built.window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, {20.0f, 15.0f}});
	built.type("i");
	CHECK(built.state("country")["open"] == true);
	built.window->routeKey({affresco::Key::Down, 0, affresco::Action::Press, {}});
	built.window->render();
	CHECK(built.state("country")["index"] == 1);
	built.window->routeKey({affresco::Key::Enter, 0, affresco::Action::Press, {}});
	built.window->render();
	CHECK(built.state("country")["value"] == "Ireland"); // the second of Italy, Ireland, Spain and Iceland
	CHECK(built.state("country")["open"] == false);
}

TEST_CASE("form: a passwordMeter names the strength") {
	Built built(R"json({ "format": "affrescoui-screen", "version": 2, "state": {},
		"root": { "id": "root", "children": [{ "id": "meter", "use": "passwordMeter",
		"with": { "password": "Abcdefgh123!", "labels": { "strong": "Strong" } } }] } })json");
	CHECK(affresco::accessibleName(*built.screen.find("meter")) == "Strong");
}
