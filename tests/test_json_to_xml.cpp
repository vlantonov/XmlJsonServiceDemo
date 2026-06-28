#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "xmljson/conversion_options.hpp"
#include "xmljson/converter.hpp"
#include "xmljson/error.hpp"

namespace {

void assert_xml_parses(const std::string& xml, pugi::xml_document& doc) {
	const pugi::xml_parse_result result = doc.load_string(xml.c_str());
	ASSERT_TRUE(result) << result.description() << " at offset " << result.offset;
}

}  // namespace

TEST(JsonToXml, SingleKeyedObject_BecomesElement) {
	xmljson::Converter converter;
	const std::string xml = converter.json_to_xml(R"({"a":"hi"})");

	pugi::xml_document doc;
	assert_xml_parses(xml, doc);

	const pugi::xml_node root = doc.document_element();
	ASSERT_TRUE(root);
	EXPECT_STREQ(root.name(), "a");
	EXPECT_STREQ(root.child_value(), "hi");
}

TEST(JsonToXml, MultiKeyedRoot_ThrowsUnsupportedShape) {
	xmljson::Converter converter;
	try {
		(void)converter.json_to_xml(R"({"a":1,"b":2})");
		FAIL() << "Expected ConversionError";
	} catch (const xmljson::ConversionError& ex) {
		EXPECT_EQ(ex.code(), xmljson::ErrorCode::UnsupportedShape);
	} catch (...) {
		FAIL() << "Expected xmljson::ConversionError";
	}
}

TEST(JsonToXml, RootArrayValue_ThrowsUnsupportedShape) {
	xmljson::Converter converter;
	try {
		(void)converter.json_to_xml(R"({"a":[1,2]})");
		FAIL() << "Expected ConversionError";
	} catch (const xmljson::ConversionError& ex) {
		EXPECT_EQ(ex.code(), xmljson::ErrorCode::UnsupportedShape);
	} catch (...) {
		FAIL() << "Expected xmljson::ConversionError";
	}
}

TEST(JsonToXml, ArrayValuesBecomeRepeatedSiblings) {
	xmljson::Converter converter;
	const std::string xml = converter.json_to_xml(R"({"r":{"b":[1,2,3]}})");

	pugi::xml_document doc;
	assert_xml_parses(xml, doc);

	const pugi::xml_node root = doc.document_element();
	ASSERT_TRUE(root);
	ASSERT_STREQ(root.name(), "r");

	std::vector<std::string> values;
	for (const pugi::xml_node child : root.children("b")) {
		values.emplace_back(child.child_value());
	}

	const std::vector<std::string> expected = {"1", "2", "3"};
	EXPECT_EQ(values, expected);
}

TEST(JsonToXml, AttributesEmittedFromAtPrefix) {
	xmljson::Converter converter;
	const std::string xml = converter.json_to_xml(R"({"a":{"@x":"1","#text":"hi"}})");

	pugi::xml_document doc;
	assert_xml_parses(xml, doc);

	const pugi::xml_node root = doc.document_element();
	ASSERT_TRUE(root);
	EXPECT_STREQ(root.name(), "a");
	EXPECT_STREQ(root.attribute("x").value(), "1");
	EXPECT_STREQ(root.child_value(), "hi");
}

TEST(JsonToXml, NumericValuesEmittedAsText) {
	xmljson::Converter converter;

	{
		const std::string xml = converter.json_to_xml(R"({"a":42})");
		pugi::xml_document doc;
		assert_xml_parses(xml, doc);
		EXPECT_STREQ(doc.document_element().child_value(), "42");
	}

	{
		const std::string xml = converter.json_to_xml(R"({"a":3.14})");
		pugi::xml_document doc;
		assert_xml_parses(xml, doc);
		EXPECT_STREQ(doc.document_element().child_value(), "3.14");
	}
}

TEST(JsonToXml, BoolValuesEmittedAsTrueFalse) {
	xmljson::Converter converter;

	{
		const std::string xml = converter.json_to_xml(R"({"a":true})");
		pugi::xml_document doc;
		assert_xml_parses(xml, doc);
		EXPECT_STREQ(doc.document_element().child_value(), "true");
	}

	{
		const std::string xml = converter.json_to_xml(R"({"a":false})");
		pugi::xml_document doc;
		assert_xml_parses(xml, doc);
		EXPECT_STREQ(doc.document_element().child_value(), "false");
	}
}

TEST(JsonToXml, NullValueEmitsEmptyElement) {
	xmljson::Converter converter;
	const std::string xml = converter.json_to_xml(R"({"a":null})");

	pugi::xml_document doc;
	assert_xml_parses(xml, doc);

	const pugi::xml_node root = doc.document_element();
	ASSERT_TRUE(root);
	EXPECT_STREQ(root.name(), "a");
	EXPECT_TRUE(root.first_child().empty());
	EXPECT_TRUE(root.attributes_begin() == root.attributes_end());
}

TEST(JsonToXml, NestedArraysRejected) {
	xmljson::Converter converter;
	try {
		(void)converter.json_to_xml(R"({"a":{"b":[[1,2],[3]]}})");
		FAIL() << "Expected ConversionError";
	} catch (const xmljson::ConversionError& ex) {
		EXPECT_EQ(ex.code(), xmljson::ErrorCode::UnsupportedShape);
	} catch (...) {
		FAIL() << "Expected xmljson::ConversionError";
	}
}

TEST(JsonToXml, XmlDeclarationEmittedByDefault) {
	xmljson::Converter converter;
	const std::string xml = converter.json_to_xml(R"({"a":"hi"})");
	EXPECT_EQ(xml.rfind("<?xml", 0), 0U);
}

TEST(JsonToXml, XmlDeclarationSuppressed) {
	xmljson::ConversionOptions options;
	options.emit_xml_declaration = false;
	xmljson::Converter converter(options);

	const std::string xml = converter.json_to_xml(R"({"a":"hi"})");
	EXPECT_NE(xml.rfind("<?xml", 0), 0U);
}

TEST(JsonToXml, PrettyPrintIndents) {
	xmljson::ConversionOptions options;
	options.pretty_print = true;
	options.indent_width = 2;
	xmljson::Converter converter(options);

	const std::string xml = converter.json_to_xml(R"({"a":{"b":"x"}})");
	EXPECT_NE(xml.find('\n'), std::string::npos);
}

TEST(JsonToXml, MalformedJson_Throws) {
	xmljson::Converter converter;
	try {
		(void)converter.json_to_xml("{\"a\":}");
		FAIL() << "Expected ConversionError";
	} catch (const xmljson::ConversionError& ex) {
		EXPECT_EQ(ex.code(), xmljson::ErrorCode::MalformedInput);
	} catch (...) {
		FAIL() << "Expected xmljson::ConversionError";
	}
}

TEST(JsonToXml, CustomAttributePrefix) {
	xmljson::ConversionOptions options;
	options.attribute_prefix = "_";
	xmljson::Converter converter(options);

	const std::string xml = converter.json_to_xml(R"({"a":{"_x":"1","_lang":"en","#text":"ok"}})");

	pugi::xml_document doc;
	assert_xml_parses(xml, doc);

	const pugi::xml_node root = doc.document_element();
	ASSERT_TRUE(root);
	EXPECT_STREQ(root.name(), "a");
	EXPECT_STREQ(root.attribute("x").value(), "1");
	EXPECT_STREQ(root.attribute("lang").value(), "en");
	EXPECT_STREQ(root.child_value(), "ok");
}