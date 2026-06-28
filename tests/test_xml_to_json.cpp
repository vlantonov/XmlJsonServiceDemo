#include <string>
#include <utility>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "xmljson/conversion_options.hpp"
#include "xmljson/converter.hpp"
#include "xmljson/error.hpp"

TEST(XmlToJson, EmptyElement_BecomesNull) {
	xmljson::Converter converter;
	const nlohmann::json actual = nlohmann::json::parse(converter.xml_to_json("<a/>"));
	const nlohmann::json expected = nlohmann::json::parse(R"({"a":null})");
	EXPECT_EQ(actual, expected);
}

TEST(XmlToJson, TextOnlyElement_BecomesString) {
	xmljson::Converter converter;
	const nlohmann::json actual = nlohmann::json::parse(converter.xml_to_json("<a>hi</a>"));
	const nlohmann::json expected = nlohmann::json::parse(R"({"a":"hi"})");
	EXPECT_EQ(actual, expected);
}

TEST(XmlToJson, ElementWithAttribute_BecomesObject) {
	xmljson::Converter converter;
	const nlohmann::json actual = nlohmann::json::parse(converter.xml_to_json("<a x=\"1\">hi</a>"));
	const nlohmann::json expected = nlohmann::json::parse(R"({"a":{"@x":"1","#text":"hi"}})");
	EXPECT_EQ(actual, expected);
}

TEST(XmlToJson, RepeatedSiblings_BecomeArray) {
	xmljson::Converter converter;
	const nlohmann::json actual = nlohmann::json::parse(converter.xml_to_json("<r><b>1</b><b>2</b><b>3</b></r>"));
	const nlohmann::json expected = nlohmann::json::parse(R"({"r":{"b":["1","2","3"]}})");
	EXPECT_EQ(actual, expected);
}

TEST(XmlToJson, SingleChild_NotArrayByDefault) {
	xmljson::Converter converter;
	const nlohmann::json actual = nlohmann::json::parse(converter.xml_to_json("<r><b>1</b></r>"));
	const nlohmann::json expected = nlohmann::json::parse(R"({"r":{"b":"1"}})");
	EXPECT_EQ(actual, expected);
}

TEST(XmlToJson, AlwaysArrayKeys_ForcesArray) {
	xmljson::ConversionOptions options;
	options.always_array_keys = {"b"};
	xmljson::Converter converter(options);

	const nlohmann::json actual = nlohmann::json::parse(converter.xml_to_json("<r><b>1</b></r>"));
	const nlohmann::json expected = nlohmann::json::parse(R"({"r":{"b":["1"]}})");
	EXPECT_EQ(actual, expected);
}

TEST(XmlToJson, ForceArrayKeys_ForcesAllArrays) {
	xmljson::ConversionOptions options;
	options.force_array_keys = true;
	xmljson::Converter converter(options);

	const nlohmann::json actual = nlohmann::json::parse(converter.xml_to_json("<r><b>1</b></r>"));
	const nlohmann::json expected = nlohmann::json::parse(R"({"r":{"b":["1"]}})");
	EXPECT_EQ(actual, expected);
}

TEST(XmlToJson, NamespacesPreservedInKeys) {
	xmljson::Converter converter;
	const nlohmann::json actual = nlohmann::json::parse(
		converter.xml_to_json("<ns:el xmlns:ns=\"http://x\"/>"));

	ASSERT_TRUE(actual.contains("ns:el"));
	ASSERT_TRUE(actual.at("ns:el").is_object());
	ASSERT_TRUE(actual.at("ns:el").contains("@xmlns:ns"));
	EXPECT_EQ(actual.at("ns:el").at("@xmlns:ns"), "http://x");
}

TEST(XmlToJson, UnicodeRoundTripsThrough) {
	xmljson::Converter converter;
	const std::string xml = u8"<a label=\"naïve\">Привет мир</a>";
	const nlohmann::json actual = nlohmann::json::parse(converter.xml_to_json(xml));

	ASSERT_TRUE(actual.contains("a"));
	ASSERT_TRUE(actual.at("a").is_object());
	EXPECT_EQ(actual.at("a").at("@label"), u8"naïve");
	EXPECT_EQ(actual.at("a").at("#text"), u8"Привет мир");
}

TEST(XmlToJson, CDataPreservedAsText) {
	xmljson::Converter converter;
	const nlohmann::json actual = nlohmann::json::parse(converter.xml_to_json("<a><![CDATA[<raw>]]></a>"));
	const nlohmann::json expected = nlohmann::json::parse(R"({"a":"<raw>"})");
	EXPECT_EQ(actual, expected);
}

TEST(XmlToJson, MixedContentConcatenated) {
	xmljson::Converter converter;
	const nlohmann::json actual = nlohmann::json::parse(converter.xml_to_json("<a>foo<b>x</b>bar</a>"));

	ASSERT_TRUE(actual.contains("a"));
	ASSERT_TRUE(actual.at("a").is_object());
	EXPECT_EQ(actual.at("a").at("b"), "x");
	EXPECT_EQ(actual.at("a").at("#text"), "foobar");
}

TEST(XmlToJson, NestedThreeLevels) {
	xmljson::Converter converter;
	const nlohmann::json actual = nlohmann::json::parse(converter.xml_to_json("<a><b><c>z</c></b></a>"));
	const nlohmann::json expected = nlohmann::json::parse(R"({"a":{"b":{"c":"z"}}})");
	EXPECT_EQ(actual, expected);
}

TEST(XmlToJson, CommentsAndPIsDropped) {
	xmljson::Converter converter;
	const nlohmann::json actual =
		nlohmann::json::parse(converter.xml_to_json("<a><!-- comment --><?pi data?><b>1</b></a>"));
	const nlohmann::json expected = nlohmann::json::parse(R"({"a":{"b":"1"}})");
	EXPECT_EQ(actual, expected);
}

TEST(XmlToJson, CustomAttributePrefix) {
	xmljson::ConversionOptions options;
	options.attribute_prefix = "_";
	xmljson::Converter converter(options);

	const nlohmann::json actual = nlohmann::json::parse(converter.xml_to_json("<a x=\"1\">hi</a>"));
	const nlohmann::json expected = nlohmann::json::parse(R"({"a":{"_x":"1","#text":"hi"}})");
	EXPECT_EQ(actual, expected);
}

TEST(XmlToJson, CustomTextKey) {
	xmljson::ConversionOptions options;
	options.text_key = "value";
	xmljson::Converter converter(options);

	const nlohmann::json actual = nlohmann::json::parse(converter.xml_to_json("<a>foo<b>x</b>bar</a>"));

	ASSERT_TRUE(actual.contains("a"));
	ASSERT_TRUE(actual.at("a").is_object());
	EXPECT_EQ(actual.at("a").at("b"), "x");
	EXPECT_EQ(actual.at("a").at("value"), "foobar");
}

TEST(XmlToJson, MalformedXml_ThrowsMalformedInput) {
	xmljson::Converter converter;
	try {
		(void)converter.xml_to_json("<a><b></a>");
		FAIL() << "Expected ConversionError";
	} catch (const xmljson::ConversionError& ex) {
		EXPECT_EQ(ex.code(), xmljson::ErrorCode::MalformedInput);
	} catch (...) {
		FAIL() << "Expected xmljson::ConversionError";
	}
}

TEST(XmlToJson, PrettyPrintProducesIndentedJson) {
	xmljson::ConversionOptions options;
	options.pretty_print = true;
	options.indent_width = 2;
	xmljson::Converter converter(options);

	const std::string output = converter.xml_to_json("<a><b>1</b></a>");
	EXPECT_NE(output.find('\n'), std::string::npos);
	const nlohmann::json actual = nlohmann::json::parse(output);
	const nlohmann::json expected = nlohmann::json::parse(R"({"a":{"b":"1"}})");
	EXPECT_EQ(actual, expected);
}