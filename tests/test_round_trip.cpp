#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <pugixml.hpp>

#include "xmljson/converter.hpp"
#include "xmljson/error.hpp"

namespace {

std::string load_fixture(const char* name) {
	namespace fs = std::filesystem;
	fs::path current = fs::current_path();

	for (int depth = 0; depth < 8; ++depth) {
		const fs::path candidate = current / "tests" / "fixtures" / name;
		if (fs::exists(candidate)) {
			std::ifstream input(candidate, std::ios::binary);
			if (!input) {
				ADD_FAILURE() << "failed to open fixture: " << candidate.string();
				return {};
			}
			std::ostringstream buffer;
			buffer << input.rdbuf();
			return buffer.str();
		}
		if (!current.has_parent_path()) {
			break;
		}
		current = current.parent_path();
	}

	ADD_FAILURE() << "fixture not found: " << name;
	return {};
}

std::vector<std::pair<std::string, std::string>> sorted_attributes(const pugi::xml_node& node) {
	std::vector<std::pair<std::string, std::string>> attributes;
	for (const pugi::xml_attribute attribute : node.attributes()) {
		attributes.emplace_back(attribute.name(), attribute.value());
	}
	std::sort(attributes.begin(), attributes.end());
	return attributes;
}

std::string concatenated_direct_text(const pugi::xml_node& node) {
	std::string text;
	for (const pugi::xml_node child : node.children()) {
		if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata) {
			text.append(child.value());
		}
	}
	return text;
}

std::vector<pugi::xml_node> element_children(const pugi::xml_node& node) {
	std::vector<pugi::xml_node> children;
	for (const pugi::xml_node child : node.children()) {
		if (child.type() == pugi::node_element) {
			children.push_back(child);
		}
	}
	return children;
}

bool xml_semantically_equal(const pugi::xml_node& a, const pugi::xml_node& b) {
	if (std::string(a.name()) != std::string(b.name())) {
		return false;
	}

	if (sorted_attributes(a) != sorted_attributes(b)) {
		return false;
	}

	if (concatenated_direct_text(a) != concatenated_direct_text(b)) {
		return false;
	}

	const std::vector<pugi::xml_node> children_a = element_children(a);
	const std::vector<pugi::xml_node> children_b = element_children(b);

	if (children_a.size() != children_b.size()) {
		return false;
	}

	std::vector<bool> matched(children_b.size(), false);
	for (const pugi::xml_node child_a : children_a) {
		bool found_match = false;
		for (std::size_t i = 0; i < children_b.size(); ++i) {
			if (matched[i]) {
				continue;
			}
			if (xml_semantically_equal(child_a, children_b[i])) {
				matched[i] = true;
				found_match = true;
				break;
			}
		}
		if (!found_match) {
			return false;
		}
	}

	return true;
}

void assert_xml_parses(const std::string& xml, pugi::xml_document& doc) {
	const pugi::xml_parse_result result = doc.load_string(xml.c_str());
	ASSERT_TRUE(result) << result.description() << " at offset " << result.offset;
}

}  // namespace

TEST(RoundTrip, XmlJsonXml_Invoice) {
	const std::string source_xml = load_fixture("sample_invoice.xml");
	xmljson::Converter converter;

	const std::string json = converter.xml_to_json(source_xml);
	const std::string round_tripped_xml = converter.json_to_xml(json);

	pugi::xml_document source_doc;
	pugi::xml_document round_doc;
	assert_xml_parses(source_xml, source_doc);
	assert_xml_parses(round_tripped_xml, round_doc);

	EXPECT_TRUE(xml_semantically_equal(source_doc.document_element(), round_doc.document_element()));
}

TEST(RoundTrip, JsonXmlJson_Invoice) {
	const std::string source_json = load_fixture("sample_invoice.json");
	xmljson::Converter converter;

	const std::string xml = converter.json_to_xml(source_json);
	const std::string round_tripped_json = converter.xml_to_json(xml);

	const nlohmann::json original = nlohmann::json::parse(source_json);
	const nlohmann::json round_tripped = nlohmann::json::parse(round_tripped_json);
	EXPECT_EQ(original, round_tripped);
}

TEST(RoundTrip, XmlJsonXml_Attributes) {
	const std::string source_xml = load_fixture("attributes.xml");
	xmljson::Converter converter;

	const std::string json = converter.xml_to_json(source_xml);
	const std::string round_tripped_xml = converter.json_to_xml(json);

	pugi::xml_document source_doc;
	pugi::xml_document round_doc;
	assert_xml_parses(source_xml, source_doc);
	assert_xml_parses(round_tripped_xml, round_doc);

	EXPECT_TRUE(xml_semantically_equal(source_doc.document_element(), round_doc.document_element()));
}

TEST(RoundTrip, XmlJsonXml_MixedContent) {
	const std::string source_xml = load_fixture("mixed_content.xml");
	xmljson::Converter converter;

	const std::string json = converter.xml_to_json(source_xml);
	const std::string round_tripped_xml = converter.json_to_xml(json);

	pugi::xml_document doc;
	assert_xml_parses(round_tripped_xml, doc);

	const pugi::xml_node root = doc.document_element();
	ASSERT_TRUE(root);
	EXPECT_STREQ(root.name(), "doc");

	// Mixed-content interleaving is lossy by design; concatenated text is preserved.
	EXPECT_STREQ(concatenated_direct_text(root).c_str(), "headtail");

	const pugi::xml_node em = root.child("em");
	ASSERT_TRUE(em);
	EXPECT_STREQ(em.child_value(), "x");
}

TEST(RoundTrip, MalformedFixture_Throws) {
	const std::string malformed_xml = load_fixture("malformed.xml");
	xmljson::Converter converter;

	try {
		(void)converter.xml_to_json(malformed_xml);
		FAIL() << "Expected ConversionError";
	} catch (const xmljson::ConversionError& ex) {
		EXPECT_EQ(ex.code(), xmljson::ErrorCode::MalformedInput);
	} catch (...) {
		FAIL() << "Expected xmljson::ConversionError";
	}
}