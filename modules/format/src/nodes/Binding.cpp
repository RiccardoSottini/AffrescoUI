/// @file
/// @brief format::Binding: values compiled once, then evaluated on the expression machine as evaluate() would.

#include "Binding.hpp"

#include "ExpressionMachine.hpp"

#include <utility>
#include <vector>

namespace affresco::format {

namespace detail {

/// @brief A value written in a document, compiled: what Machine::evaluateDocument() does to it, decided once.
struct CompiledValue {
	/// What evaluating it does.
	enum class Kind : std::uint8_t {
		Constant, ///< Gives the value as written (a part of an object or a list that has dynamic parts too).
		Template, ///< A text with placeholders.
		Operator, ///< One `$` operator.
		Object,   ///< An object with dynamic parts: each key evaluated.
		List,     ///< A list with dynamic parts: each item evaluated.
	};
	Kind kind = Kind::Constant;            ///< What it does.
	const json* written = nullptr;         ///< The value as written; outlives this.
	std::shared_ptr<const Template> shape; ///< Kind::Template: split; null if it fails (reported where it is used).
	Node program;                          ///< Kind::Operator.
	std::vector<std::string_view> keys;    ///< Kind::Object: each part's key (views into the value as written).
	std::vector<CompiledValue> parts;      ///< Kind::Object and Kind::List.
};

namespace {

/// @brief Compiles a value.
/// @param written The value as written; outlives the result.
/// @return Its compiled form (Kind::Constant if evaluating it gives it back).
CompiledValue compileValue(const json& written) {
	CompiledValue value;
	value.written = &written;
	if(written.is_string()) {
		const std::string& text = written.get_ref<const std::string&>();
		if(!text.contains("{{")) return value;
		value.kind = CompiledValue::Kind::Template;
		static const std::string compiling = "compiling";
		std::shared_ptr<const Template> shape = templateOf(text, Place(compiling));
		if(!shape->failure) value.shape = std::move(shape); // else the error is reported at the place it is used
		return value;
	}
	if(written.is_object() && written.size() == 1 && written.begin().key().starts_with('$')) {
		value.kind = CompiledValue::Kind::Operator;
		value.program = lower(written);
		return value;
	}
	if(!written.is_object() && !written.is_array()) return value;

	bool dynamic = false;
	for(const auto& [key, item] : written.items()) {
		CompiledValue part = compileValue(item);
		dynamic = dynamic || part.kind != CompiledValue::Kind::Constant;
		if(written.is_object()) value.keys.push_back(key);
		value.parts.push_back(std::move(part));
	}
	if(!dynamic) {
		CompiledValue constant;
		constant.written = &written;
		return constant;
	}
	value.kind = written.is_object() ? CompiledValue::Kind::Object : CompiledValue::Kind::List;
	return value;
}

/// @brief Evaluates a compiled value, as Machine::evaluateDocument() evaluates the value as written.
/// @param machine The machine.
/// @param value The compiled value.
/// @param place Its place.
/// @return The value.
/// @throws DocumentError If something in it is invalid.
json evaluateValue(Machine& machine, const CompiledValue& value, const Place& place) {
	switch(value.kind) {
	case CompiledValue::Kind::Constant:
		return *value.written;
	case CompiledValue::Kind::Template:
		if(value.shape) return machine.runTemplate(*value.shape, place).toJson();
		return machine.runTemplate(value.written->get_ref<const std::string&>(), place).toJson();
	case CompiledValue::Kind::Operator:
		return machine.run(value.program, place).toJson();
	case CompiledValue::Kind::Object: {
		json result = json::object();
		for(std::size_t i = 0; i < value.parts.size(); ++i)
			result[std::string(value.keys[i])] = evaluateValue(machine, value.parts[i], Place(place, value.keys[i]));
		return result;
	}
	case CompiledValue::Kind::List:
		break;
	}
	json result = json::array();
	for(std::size_t i = 0; i < value.parts.size(); ++i)
		result.push_back(evaluateValue(machine, value.parts[i], Place(place, i)));
	return result;
}

} // namespace

} // namespace detail

Binding::Binding(const json& written, std::unique_ptr<const detail::CompiledValue> compiled) noexcept
    : written_(&written), compiled_(std::move(compiled)) {}

Binding::Binding(const json& written) : written_(&written) {
	detail::CompiledValue value = detail::compileValue(written);
	if(value.kind != detail::CompiledValue::Kind::Constant)
		compiled_ = std::make_unique<const detail::CompiledValue>(std::move(value));
}

Binding Binding::constant(const json& written) { return {written, nullptr}; }

Binding::Binding(Binding&&) noexcept = default;
Binding& Binding::operator=(Binding&&) noexcept = default;
Binding::~Binding() = default;

json Binding::evaluate(const Context& context, const std::string& where, std::string_view key,
                       std::string_view name) const {
	if(!compiled_) return *written_;
	detail::Machine machine(context);
	const detail::Place node(where);
	if(key.empty()) return detail::evaluateValue(machine, *compiled_, node);
	const detail::Place property(node, key);
	if(name.empty()) return detail::evaluateValue(machine, *compiled_, property);
	return detail::evaluateValue(machine, *compiled_, detail::Place(property, name));
}

} // namespace affresco::format
