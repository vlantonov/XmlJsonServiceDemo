#include "xmljson/converter.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace xmljson {

std::string xml_to_json_impl(std::string_view xml, const ConversionOptions& options);
std::string json_to_xml_impl(std::string_view json, const ConversionOptions& options);

Converter::Converter() = default;

Converter::Converter(ConversionOptions options) : options_(std::move(options)) {}

// cppcheck-suppress passedByValue
std::string Converter::xml_to_json(std::string_view xml) const {
	return xml_to_json_impl(xml, options_);
}

// cppcheck-suppress passedByValue
std::string Converter::json_to_xml(std::string_view json) const {
	return json_to_xml_impl(json, options_);
}

const ConversionOptions& Converter::options() const noexcept {
	return options_;
}

}  // namespace xmljson