#pragma once
#include <string>
#include <string_view>
#include "xmljson/conversion_options.hpp"

namespace xmljson {

/// \brief Performs bidirectional conversion between XML and JSON strings.
class Converter {
 public:
    /// \brief Constructs a Converter with default options.
    Converter();
    
    /// \brief Constructs a Converter with the specified options.
    explicit Converter(ConversionOptions options);

    /// \brief Converts an XML string to JSON. Throws ConversionError.
    std::string xml_to_json(std::string_view xml) const;
    
    /// \brief Converts a JSON string to XML. Throws ConversionError.
    std::string json_to_xml(std::string_view json) const;

    /// \brief Returns the active conversion options.
    const ConversionOptions& options() const noexcept;

 private:
    ConversionOptions options_;
};

}  // namespace xmljson
