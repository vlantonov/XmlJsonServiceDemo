#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>

#include "xmljson/config.hpp"
#include "xmljson/error.hpp"

namespace xmljson {
void apply_cli_overrides(ServerConfig& target, int argc, char** argv);
}

namespace {

using ::testing::HasSubstr;

void expect_same_config(const xmljson::ServerConfig& actual, const xmljson::ServerConfig& expected) {
	EXPECT_EQ(actual.host, expected.host);
	EXPECT_EQ(actual.port, expected.port);
	EXPECT_EQ(actual.base_threads, expected.base_threads);
	EXPECT_EQ(actual.max_threads, expected.max_threads);
	EXPECT_EQ(actual.max_queued, expected.max_queued);
	EXPECT_EQ(actual.max_body_bytes, expected.max_body_bytes);
	EXPECT_EQ(actual.read_timeout_seconds, expected.read_timeout_seconds);
	EXPECT_EQ(actual.write_timeout_seconds, expected.write_timeout_seconds);
	EXPECT_EQ(actual.log_level, expected.log_level);
	EXPECT_EQ(actual.log_file, expected.log_file);
	EXPECT_EQ(actual.config_path, expected.config_path);
}

void expect_invalid_config(const std::function<void()>& fn) {
	try {
		fn();
		FAIL() << "Expected ConversionError";
	} catch (const xmljson::ConversionError& ex) {
		EXPECT_EQ(ex.code(), xmljson::ErrorCode::InvalidConfig);
	} catch (...) {
		FAIL() << "Expected xmljson::ConversionError";
	}
}

struct ArgvBuilder {
	explicit ArgvBuilder(std::initializer_list<std::string> values) : args(values) {
		for (auto& arg : args) {
			argv.push_back(arg.data());
		}
	}

	int argc() const { return static_cast<int>(argv.size()); }
	char** data() { return argv.data(); }

	std::vector<std::string> args;
	std::vector<char*> argv;
};

class ConfigTest : public ::testing::Test {
protected:
	std::filesystem::path write_temp_file(const std::string& content) {
		const auto unique = "xmljson_config_test_" + std::to_string(next_id_++) + ".json";
		const std::filesystem::path path = std::filesystem::temp_directory_path() / unique;
		std::ofstream out(path);
		out << content;
		out.close();
		temp_files_.push_back(path);
		return path;
	}

	void TearDown() override {
		for (const auto& file : temp_files_) {
			std::error_code ec;
			std::filesystem::remove(file, ec);
		}
	}

private:
	std::vector<std::filesystem::path> temp_files_;
	int next_id_ = 1;
};

}  // namespace

TEST(Config, Defaults_Applied_When_NoFile) {
	const xmljson::ServerConfig loaded = xmljson::load_config_from_file("");
	const xmljson::ServerConfig defaults;
	expect_same_config(loaded, defaults);
}

TEST(Config, Defaults_When_File_Missing_NotRequired) {
	const auto missing = (std::filesystem::temp_directory_path() / "xmljson_config_missing.json").string();
	const xmljson::ServerConfig loaded = xmljson::load_config_from_file(missing, false);
	const xmljson::ServerConfig defaults;
	expect_same_config(loaded, defaults);
}

TEST(Config, Throws_When_File_Missing_Required) {
	const auto missing = (std::filesystem::temp_directory_path() / "xmljson_config_missing_required.json").string();
	try {
		(void)xmljson::load_config_from_file(missing, true);
		FAIL() << "Expected ConversionError";
	} catch (const xmljson::ConversionError& ex) {
		EXPECT_EQ(ex.code(), xmljson::ErrorCode::InvalidConfig);
		EXPECT_THAT(std::string(ex.what()), HasSubstr("config file not found"));
	} catch (...) {
		FAIL() << "Expected xmljson::ConversionError";
	}
}

TEST_F(ConfigTest, Loads_Valid_Json_File) {
	const std::filesystem::path path = write_temp_file(
		R"({"host":"127.0.0.1","port":9000,"log_level":"debug"})");
	const xmljson::ServerConfig loaded = xmljson::load_config_from_file(path.string(), true);

	EXPECT_EQ(loaded.host, "127.0.0.1");
	EXPECT_EQ(loaded.port, 9000);
	EXPECT_EQ(loaded.log_level, "debug");
	EXPECT_EQ(loaded.config_path, path.string());
}

TEST_F(ConfigTest, Malformed_Json_Throws_InvalidConfig) {
	const std::filesystem::path path = write_temp_file("{ this is not json");
	expect_invalid_config([&]() { (void)xmljson::load_config_from_file(path.string(), true); });
}

TEST_F(ConfigTest, Wrong_Type_Throws) {
	const std::filesystem::path path = write_temp_file(R"({"port":"oops"})");
	expect_invalid_config([&]() { (void)xmljson::load_config_from_file(path.string(), true); });
}

TEST_F(ConfigTest, Out_Of_Range_Port_Throws) {
	const std::filesystem::path path = write_temp_file(R"({"port":70000})");
	expect_invalid_config([&]() { (void)xmljson::load_config_from_file(path.string(), true); });
}

TEST_F(ConfigTest, Negative_Threads_Throws) {
	const std::filesystem::path path = write_temp_file(R"({"base_threads":-1})");
	expect_invalid_config([&]() { (void)xmljson::load_config_from_file(path.string(), true); });
}

TEST_F(ConfigTest, Unknown_Key_Warns_But_Does_Not_Throw) {
	const std::filesystem::path path = write_temp_file(R"({"unknown_key":123})");

	const auto previous_logger = spdlog::default_logger();
	const auto previous_level = spdlog::default_logger()->level();
	std::ostringstream log_capture;
	auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(log_capture);
	auto logger = std::make_shared<spdlog::logger>("config_test_logger", sink);

	spdlog::set_default_logger(logger);
	spdlog::set_level(spdlog::level::warn);

	const xmljson::ServerConfig loaded = xmljson::load_config_from_file(path.string(), true);
	logger->flush();

	spdlog::set_default_logger(previous_logger);
	spdlog::set_level(previous_level);

	EXPECT_EQ(loaded.port, xmljson::ServerConfig{}.port);
	EXPECT_THAT(log_capture.str(), HasSubstr("unknown config key"));
}

TEST(Config, Cli_Help_Flag_Sets_Show_Help) {
	ArgvBuilder argv({"prog", "--help", "--port", "9000"});
	const xmljson::CliParseResult parsed = xmljson::parse_cli(argv.argc(), argv.data());
	EXPECT_TRUE(parsed.show_help);
}

TEST(Config, Cli_Version_Flag_Sets_Show_Version) {
	ArgvBuilder argv({"prog", "--version"});
	const xmljson::CliParseResult parsed = xmljson::parse_cli(argv.argc(), argv.data());
	EXPECT_TRUE(parsed.show_version);
}

TEST(Config, Cli_Port_Override) {
	ArgvBuilder argv({"prog", "--port", "9000"});
	const xmljson::CliParseResult parsed = xmljson::parse_cli(argv.argc(), argv.data());
	EXPECT_EQ(parsed.config.port, 9000);
}

TEST(Config, Cli_EqualsForm) {
	ArgvBuilder argv({"prog", "--port=9000"});
	const xmljson::CliParseResult parsed = xmljson::parse_cli(argv.argc(), argv.data());
	EXPECT_EQ(parsed.config.port, 9000);
}

TEST(Config, Cli_Threads_Alias) {
	ArgvBuilder argv({"prog", "--threads", "4"});
	const xmljson::CliParseResult parsed = xmljson::parse_cli(argv.argc(), argv.data());
	EXPECT_EQ(parsed.config.base_threads, 4);
}

TEST(Config, Cli_LogLevel) {
	ArgvBuilder argv({"prog", "--log-level", "debug"});
	const xmljson::CliParseResult parsed = xmljson::parse_cli(argv.argc(), argv.data());
	EXPECT_EQ(parsed.config.log_level, "debug");
}

TEST(Config, Cli_LogFile) {
	ArgvBuilder argv({"prog", "--log-file", "/tmp/foo.log"});
	const xmljson::CliParseResult parsed = xmljson::parse_cli(argv.argc(), argv.data());
	EXPECT_EQ(parsed.config.log_file, "/tmp/foo.log");
}

TEST(Config, Cli_Unknown_Flag_Throws) {
	ArgvBuilder argv({"prog", "--does-not-exist"});
	expect_invalid_config([&]() { (void)xmljson::parse_cli(argv.argc(), argv.data()); });
}

TEST(Config, Cli_Missing_Value_Throws) {
	ArgvBuilder argv({"prog", "--port"});
	expect_invalid_config([&]() { (void)xmljson::parse_cli(argv.argc(), argv.data()); });
}

TEST(Config, Cli_Non_Numeric_Port_Throws) {
	ArgvBuilder argv({"prog", "--port", "abc"});
	expect_invalid_config([&]() { (void)xmljson::parse_cli(argv.argc(), argv.data()); });
}

TEST(Config, Cli_Help_Output_Contains_Usage) {
	std::ostringstream out;
	xmljson::print_help(out, "xmljson-service");
	EXPECT_THAT(out.str(), HasSubstr("--help"));
	EXPECT_THAT(out.str(), HasSubstr("--port"));
}

TEST(Config, Cli_Version_Output_Contains_Version) {
	std::ostringstream out;
	xmljson::print_version(out);
	EXPECT_THAT(out.str(), HasSubstr("0.1.0"));
}

TEST(Config, Apply_Cli_Overrides_Internal) {
	xmljson::ServerConfig target;
	target.host = "127.0.0.1";
	target.port = 8081;
	target.log_level = "warn";

	ArgvBuilder argv({"prog", "--port", "9090"});
	xmljson::apply_cli_overrides(target, argv.argc(), argv.data());

	EXPECT_EQ(target.port, 9090);
	EXPECT_EQ(target.host, "127.0.0.1");
	EXPECT_EQ(target.log_level, "warn");
}