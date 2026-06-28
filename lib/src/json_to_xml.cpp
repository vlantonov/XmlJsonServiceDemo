#include <sstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>
#include <pugixml.hpp>

#include "xmljson/conversion_options.hpp"
#include "xmljson/error.hpp"

namespace xmljson {

namespace {

bool starts_with(const std::string& value, const std::string& prefix) {
	return value.rfind(prefix, 0) == 0;
}

std::string scalar_to_text(const nlohmann::json& value) {
	if (value.is_string()) {
		return value.get<std::string>();
	}
	if (value.is_number()) {
		return value.dump();
	}
	if (value.is_boolean()) {
		return value.get<bool>() ? "true" : "false";
	}
	if (value.is_null()) {
		return "";
	}
	throw ConversionError(ErrorCode::UnsupportedShape, "non-scalar value cannot be emitted as XML text");
}

void emit_into(pugi::xml_node parent, const std::string& name, const nlohmann::json& value,
			   const ConversionOptions& opts) {
	if (value.is_array()) {
		for (const auto& item : value) {
			if (item.is_array()) {
				throw ConversionError(ErrorCode::UnsupportedShape, "nested arrays cannot be expressed in XML");
			}
			emit_into(parent, name, item, opts);
		}
		return;
	}

	pugi::xml_node element = parent.append_child(name.c_str());

	if (value.is_object()) {
		for (auto it = value.begin(); it != value.end(); ++it) {
			const std::string key = it.key();
			const nlohmann::json& child_value = it.value();

			if (starts_with(key, opts.attribute_prefix)) {
				const std::string attribute_name = key.substr(opts.attribute_prefix.size());
				if (attribute_name.empty()) {
					throw ConversionError(ErrorCode::UnsupportedShape, "attribute key must include a name");
				}
				if (child_value.is_object() || child_value.is_array()) {
					throw ConversionError(ErrorCode::UnsupportedShape,
										  "attribute value must be a scalar in JSON->XML conversion");
				}
				const std::string text = scalar_to_text(child_value);
				element.append_attribute(attribute_name.c_str()) = text.c_str();
				continue;
			}

			if (key == opts.text_key) {
				if (child_value.is_object() || child_value.is_array()) {
					throw ConversionError(ErrorCode::UnsupportedShape,
										  "text key value must be a scalar in JSON->XML conversion");
				}
				const std::string text = scalar_to_text(child_value);
				if (!text.empty()) {
					element.append_child(pugi::node_pcdata).set_value(text.c_str());
				}
				continue;
			}

			emit_into(element, key, child_value, opts);
		}
		return;
	}

	if (value.is_null()) {
		return;
	}

	const std::string text = scalar_to_text(value);
	if (!text.empty()) {
		element.append_child(pugi::node_pcdata).set_value(text.c_str());
	}
}

nlohmann::json parse_json_or_throw(std::string_view json_text) {
	nlohmann::json parsed = nlohmann::json::parse(json_text, nullptr, false);
	if (!parsed.is_discarded()) {
		return parsed;
	}

	try {
		const nlohmann::json parsed_again = nlohmann::json::parse(json_text);
		(void)parsed_again;
	} catch (const nlohmann::json::parse_error& ex) {
		throw ConversionError(ErrorCode::MalformedInput, std::string("JSON parse error: ") + ex.what());
	}

	throw ConversionError(ErrorCode::MalformedInput, "JSON parse error: invalid JSON input");
}

}  // namespace

std::string json_to_xml_impl(std::string_view json, const ConversionOptions& options) {
	const nlohmann::json root = parse_json_or_throw(json);

	if (!root.is_object() || root.size() != 1U) {
		throw ConversionError(ErrorCode::UnsupportedShape, "JSON root must be a single-keyed object");
	}

	const auto& root_object = root.get_ref<const nlohmann::json::object_t&>();
	const auto root_it = root_object.begin();
	const std::string root_name = root_it->first;
	const nlohmann::json& root_value = root_it->second;

	if (root_value.is_array()) {
		throw ConversionError(ErrorCode::UnsupportedShape,
							  "JSON root value cannot be an array because XML requires a single root element");
	}

	pugi::xml_document doc;
	if (options.emit_xml_declaration) {
		pugi::xml_node declaration = doc.append_child(pugi::node_declaration);
		declaration.append_attribute("version") = "1.0";
		declaration.append_attribute("encoding") = "UTF-8";
	}

	emit_into(doc, root_name, root_value, options);

	unsigned int flags = 0U;
	if (options.pretty_print) {
		flags = pugi::format_indent;
	} else {
		flags = pugi::format_raw;
	}

	if (!options.emit_xml_declaration) {
		flags |= pugi::format_no_declaration;
	}

	std::ostringstream oss;
	const std::string indent = options.pretty_print ? std::string(static_cast<std::size_t>(options.indent_width), ' ') : "";
	doc.save(oss, indent.c_str(), flags);
	return oss.str();
}

}  // namespace xmljson