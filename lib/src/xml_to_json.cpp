#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <pugixml.hpp>

#include "xmljson/conversion_options.hpp"
#include "xmljson/error.hpp"

namespace xmljson {

namespace {

nlohmann::json convert_element(const pugi::xml_node& element, const ConversionOptions& options) {
	nlohmann::json object_value = nlohmann::json::object();
	bool has_attributes = false;

	for (const pugi::xml_attribute attribute : element.attributes()) {
		has_attributes = true;
		const std::string key = options.attribute_prefix + std::string(attribute.name());
		object_value[key] = std::string(attribute.value());
	}

	std::vector<std::pair<std::string, nlohmann::json>> child_elements;
	std::unordered_map<std::string, std::size_t> child_counts;
	std::string text_value;
	bool has_text_segment = false;

	for (const pugi::xml_node child : element.children()) {
		if (child.type() == pugi::node_element) {
			const std::string child_name = child.name();
			child_elements.emplace_back(child_name, convert_element(child, options));
			++child_counts[child_name];
			continue;
		}

		if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata) {
			has_text_segment = true;
			text_value.append(child.value());
		}
	}

	const bool has_child_elements = !child_elements.empty();

	if (!has_attributes && !has_child_elements && !has_text_segment) {
		return nullptr;
	}

	if (!has_attributes && !has_child_elements && has_text_segment) {
		return text_value;
	}

	std::unordered_map<std::string, std::vector<nlohmann::json>> grouped_children;
	grouped_children.reserve(child_counts.size());

	for (auto& child : child_elements) {
		grouped_children[child.first].push_back(std::move(child.second));
	}

	for (auto& entry : grouped_children) {
		const std::string& child_name = entry.first;
		const bool force_array_for_key =
			options.force_array_keys || options.always_array_keys.find(child_name) != options.always_array_keys.end();
		const bool should_be_array = force_array_for_key || child_counts[child_name] > 1U;

		if (should_be_array) {
			object_value[child_name] = std::move(entry.second);
		} else {
			object_value[child_name] = std::move(entry.second.front());
		}
	}

	if (has_text_segment) {
		object_value[options.text_key] = text_value;
	}

	return object_value;
}

}  // namespace

std::string xml_to_json_impl(std::string_view xml, const ConversionOptions& options) {
	pugi::xml_document doc;
	const pugi::xml_parse_result result = doc.load_string(std::string(xml).c_str(), pugi::parse_default | pugi::parse_cdata);

	if (result.status != pugi::status_ok) {
		std::ostringstream oss;
		oss << "XML parse error at offset " << result.offset << ": " << result.description();
		throw ConversionError(ErrorCode::MalformedInput, oss.str());
	}

	const pugi::xml_node root = doc.document_element();
	if (!root) {
		throw ConversionError(ErrorCode::MalformedInput, "XML parse error: missing root element");
	}

	nlohmann::json wrapped = nlohmann::json::object();
	wrapped[std::string(root.name())] = convert_element(root, options);
	return wrapped.dump(options.pretty_print ? options.indent_width : -1);
}

}  // namespace xmljson