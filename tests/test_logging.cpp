#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include "xmljson/error.hpp"
#include "xmljson/logging.hpp"

namespace {

class LoggingTest : public ::testing::Test {
 protected:
	void TearDown() override {
		xmljson::init_logging(xmljson::LoggingConfig{"info", ""});
		if (!temp_log_path_.empty()) {
			std::error_code ec;
			std::filesystem::remove(temp_log_path_, ec);
			temp_log_path_.clear();
		}
	}

	std::filesystem::path temp_log_path_;
};

struct ParseLevelCase {
	// cppcheck-suppress unusedStructMember
	const char* input;
	spdlog::level::level_enum expected;
};

class ParseLevelRecognizedTest : public ::testing::TestWithParam<ParseLevelCase> {};

}  // namespace

TEST_F(LoggingTest, Logger_Returns_Non_Null) {
	EXPECT_NE(xmljson::logger(), nullptr);
}

TEST_F(LoggingTest, Logger_Named_xmljson) {
	ASSERT_NE(xmljson::logger(), nullptr);
	EXPECT_EQ(xmljson::logger()->name(), "xmljson");
}

TEST_F(LoggingTest, Init_Sets_Level) {
	xmljson::init_logging(xmljson::LoggingConfig{"debug", ""});
	ASSERT_NE(xmljson::logger(), nullptr);
	EXPECT_EQ(xmljson::logger()->level(), spdlog::level::debug);
}

TEST_F(LoggingTest, Init_Trace_Level) {
	xmljson::init_logging(xmljson::LoggingConfig{"trace", ""});
	ASSERT_NE(xmljson::logger(), nullptr);
	EXPECT_EQ(xmljson::logger()->level(), spdlog::level::trace);
}

TEST_F(LoggingTest, Init_Warn_Level) {
	xmljson::init_logging(xmljson::LoggingConfig{"warn", ""});
	ASSERT_NE(xmljson::logger(), nullptr);
	EXPECT_EQ(xmljson::logger()->level(), spdlog::level::warn);
}

TEST_F(LoggingTest, Init_Unknown_Level_Throws_InvalidConfig) {
	try {
		xmljson::init_logging(xmljson::LoggingConfig{"bogus", ""});
		FAIL() << "Expected ConversionError";
	} catch (const xmljson::ConversionError& ex) {
		EXPECT_EQ(ex.code(), xmljson::ErrorCode::InvalidConfig);
	} catch (...) {
		FAIL() << "Expected ConversionError";
	}
}

TEST_P(ParseLevelRecognizedTest, parse_level_Recognizes_Standard_Levels) {
	const auto& tc = GetParam();
	SCOPED_TRACE(tc.input);
	EXPECT_EQ(xmljson::parse_level(tc.input), tc.expected);
}

INSTANTIATE_TEST_SUITE_P(
	Logging,
	ParseLevelRecognizedTest,
	::testing::Values(
		ParseLevelCase{"trace", spdlog::level::trace},
		ParseLevelCase{"debug", spdlog::level::debug},
		ParseLevelCase{"info", spdlog::level::info},
		ParseLevelCase{"warn", spdlog::level::warn},
		ParseLevelCase{"warning", spdlog::level::warn},
		ParseLevelCase{"error", spdlog::level::err},
		ParseLevelCase{"err", spdlog::level::err},
		ParseLevelCase{"critical", spdlog::level::critical},
		ParseLevelCase{"off", spdlog::level::off}));

TEST_F(LoggingTest, parse_level_Unknown_Throws) {
	try {
		(void)xmljson::parse_level("not-a-level");
		FAIL() << "Expected ConversionError";
	} catch (const xmljson::ConversionError& ex) {
		EXPECT_EQ(ex.code(), xmljson::ErrorCode::InvalidConfig);
	} catch (...) {
		FAIL() << "Expected ConversionError";
	}
}

TEST_F(LoggingTest, parse_level_Case_Insensitive) {
	EXPECT_EQ(xmljson::parse_level("INFO"), spdlog::level::info);
	EXPECT_EQ(xmljson::parse_level("Debug"), spdlog::level::debug);
}

TEST_F(LoggingTest, Init_File_Sink_Writes_To_File) {
	temp_log_path_ = std::filesystem::temp_directory_path() / "xmljson-test.log";
	std::error_code ec;
	std::filesystem::remove(temp_log_path_, ec);

	xmljson::init_logging(xmljson::LoggingConfig{"info", temp_log_path_.string()});
	const std::string unique_message = "UNIQUE_MSG_xyz";
	xmljson::logger()->info(unique_message);
	xmljson::logger()->flush();

	std::ifstream in(temp_log_path_);
	ASSERT_TRUE(in.is_open());

	std::ostringstream content_stream;
	content_stream << in.rdbuf();
	const std::string content = content_stream.str();
	EXPECT_NE(content.find(unique_message), std::string::npos);

	xmljson::init_logging(xmljson::LoggingConfig{"info", ""});
}

TEST_F(LoggingTest, Init_Multiple_Calls_Safe) {
	EXPECT_NO_THROW(xmljson::init_logging(xmljson::LoggingConfig{"info", ""}));
	EXPECT_NO_THROW(xmljson::init_logging(xmljson::LoggingConfig{"error", ""}));
	ASSERT_NE(xmljson::logger(), nullptr);
	EXPECT_EQ(xmljson::logger()->level(), spdlog::level::err);
}

TEST_F(LoggingTest, Logger_Lazy_Init_When_Init_Not_Called) {
	spdlog::drop("xmljson");
	auto log = xmljson::logger();
	ASSERT_NE(log, nullptr);
	EXPECT_EQ(log->name(), "xmljson");
	EXPECT_EQ(log->level(), spdlog::level::info);
}