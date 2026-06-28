#pragma once
#include <set>
#include <string>

namespace xmljson {

/// \brief Configuration options for XML-JSON bidirectional conversion.
struct ConversionOptions {
    std::string attribute_prefix = "@";
    std::string text_key         = "#text";
    bool        pretty_print     = false;
    int         indent_width     = 2;
    bool        force_array_keys = false;
    std::set<std::string> always_array_keys{};
    bool        emit_xml_declaration = true;   // JSON→XML: prepend <?xml version="1.0" encoding="UTF-8"?>
    std::string xml_root_default = "root";     // unused for now (only single-keyed roots allowed) but reserved
};

}  // namespace xmljson
