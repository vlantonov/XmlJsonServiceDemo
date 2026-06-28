#include "xmljson/config.hpp"

#include <filesystem>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <optional>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <vector>

#include "xmljson/error.hpp"

namespace xmljson {
namespace {

struct ParsedCliFlags {
	std::optional<std::string> host;
	std::optional<int> port;
	std::optional<int> base_threads;
	std::optional<int> max_threads;
	std::optional<int> max_queued;
	std::optional<std::size_t> max_body_bytes;
	std::optional<int> read_timeout_seconds;
	std::optional<int> write_timeout_seconds;
	std::optional<std::string> log_level;
	std::optional<std::string> log_file;
	std::optional<std::string> config_path;
	bool show_help = false;
	bool show_version = false;
};

[[noreturn]] void throw_invalid_config(const std::string& message) {
	throw ConversionError(ErrorCode::InvalidConfig, message);
}

long long parse_signed_ll(const std::string& raw, const std::string& flag) {
	if (raw.empty()) {
		throw_invalid_config("missing value for " + flag);
	}

	std::size_t parsed = 0;
	long long value = 0;
	try {
		value = std::stoll(raw, &parsed, 10);
	} catch (...) {
		throw_invalid_config("invalid numeric value for " + flag + ": " + raw);
	}
	if (parsed != raw.size()) {
		throw_invalid_config("invalid numeric value for " + flag + ": " + raw);
	}
	return value;
}

std::size_t parse_size_t_value(const std::string& raw, const std::string& flag) {
	if (raw.empty()) {
		throw_invalid_config("missing value for " + flag);
	}

	std::size_t parsed = 0;
	unsigned long long value = 0;
	try {
		value = std::stoull(raw, &parsed, 10);
	} catch (...) {
		throw_invalid_config("invalid numeric value for " + flag + ": " + raw);
	}
	if (parsed != raw.size()) {
		throw_invalid_config("invalid numeric value for " + flag + ": " + raw);
	}
	if (value > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
		throw_invalid_config("value out of range for " + flag + ": " + raw);
	}
	return static_cast<std::size_t>(value);
}

int parse_int_value(const std::string& raw, const std::string& flag) {
	const long long value = parse_signed_ll(raw, flag);
	if (value < static_cast<long long>(std::numeric_limits<int>::min()) ||
		value > static_cast<long long>(std::numeric_limits<int>::max())) {
		throw_invalid_config("value out of range for " + flag + ": " + raw);
	}
	return static_cast<int>(value);
}

std::pair<std::string, std::optional<std::string>> split_flag(const std::string& arg) {
	const std::size_t eq_pos = arg.find('=');
	if (eq_pos == std::string::npos) {
		return {arg, std::nullopt};
	}
	return {arg.substr(0, eq_pos), arg.substr(eq_pos + 1)};
}

std::string require_flag_value(int argc, char** argv, int& index, const std::string& flag,
							   const std::optional<std::string>& inline_value) {
	if (inline_value.has_value()) {
		return *inline_value;
	}
	if (index + 1 >= argc) {
		throw_invalid_config("missing value for " + flag);
	}
	++index;
	return argv[index];
}

void validate_and_apply_config_json(ServerConfig& result, const nlohmann::json& root) {
	auto require_type = [](const std::string& key, const std::string& expected) {
		throw_invalid_config("invalid type for key '" + key + "': expected " + expected);
	};

	auto get_integer_like = [&](const nlohmann::json& value, const std::string& key) -> long long {
		if (value.is_number_integer()) {
			return value.get<long long>();
		}
		if (value.is_number_unsigned()) {
			const unsigned long long u = value.get<unsigned long long>();
			if (u > static_cast<unsigned long long>(std::numeric_limits<long long>::max())) {
				throw_invalid_config("value out of range for key '" + key + "'");
			}
			return static_cast<long long>(u);
		}
		require_type(key, "integer");
		return 0;
	};

	auto get_non_negative_size_t = [&](const nlohmann::json& value, const std::string& key) -> std::size_t {
		if (value.is_number_unsigned()) {
			const unsigned long long u = value.get<unsigned long long>();
			if (u > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
				throw_invalid_config("value out of range for key '" + key + "'");
			}
			return static_cast<std::size_t>(u);
		}
		if (value.is_number_integer()) {
			const long long i = value.get<long long>();
			if (i < 0) {
				throw_invalid_config("value for key '" + key + "' must be >= 0");
			}
			if (static_cast<unsigned long long>(i) >
				static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
				throw_invalid_config("value out of range for key '" + key + "'");
			}
			return static_cast<std::size_t>(i);
		}
		require_type(key, "non-negative integer");
		return 0;
	};

	const std::vector<std::string> known_keys = {
		"host",
		"port",
		"base_threads",
		"max_threads",
		"max_queued",
		"max_body_bytes",
		"read_timeout_seconds",
		"write_timeout_seconds",
		"log_level",
		"log_file",
	};

	for (auto it = root.begin(); it != root.end(); ++it) {
		const auto found = std::find(known_keys.begin(), known_keys.end(), it.key());
		if (found == known_keys.end()) {
			spdlog::warn("unknown config key: {}", it.key());
		}
	}

	if (root.contains("host")) {
		if (!root.at("host").is_string()) {
			require_type("host", "string");
		}
		result.host = root.at("host").get<std::string>();
	}

	if (root.contains("port")) {
		const long long v = get_integer_like(root.at("port"), "port");
		if (v < 0 || v > 65535) {
			throw_invalid_config("value out of range for key 'port': expected [0, 65535]");
		}
		result.port = static_cast<int>(v);
	}

	if (root.contains("base_threads")) {
		const long long v = get_integer_like(root.at("base_threads"), "base_threads");
		if (v < 1) {
			throw_invalid_config("value out of range for key 'base_threads': expected >= 1");
		}
		if (v > static_cast<long long>(std::numeric_limits<int>::max())) {
			throw_invalid_config("value out of range for key 'base_threads'");
		}
		result.base_threads = static_cast<int>(v);
	}

	if (root.contains("max_threads")) {
		const long long v = get_integer_like(root.at("max_threads"), "max_threads");
		if (v < 0) {
			throw_invalid_config("value out of range for key 'max_threads': expected >= 0");
		}
		if (v > static_cast<long long>(std::numeric_limits<int>::max())) {
			throw_invalid_config("value out of range for key 'max_threads'");
		}
		result.max_threads = static_cast<int>(v);
	}

	if (root.contains("max_queued")) {
		const long long v = get_integer_like(root.at("max_queued"), "max_queued");
		if (v < 0) {
			throw_invalid_config("value out of range for key 'max_queued': expected >= 0");
		}
		if (v > static_cast<long long>(std::numeric_limits<int>::max())) {
			throw_invalid_config("value out of range for key 'max_queued'");
		}
		result.max_queued = static_cast<int>(v);
	}

	if (root.contains("max_body_bytes")) {
		result.max_body_bytes = get_non_negative_size_t(root.at("max_body_bytes"), "max_body_bytes");
	}

	if (root.contains("read_timeout_seconds")) {
		const long long v = get_integer_like(root.at("read_timeout_seconds"), "read_timeout_seconds");
		if (v < 0) {
			throw_invalid_config("value out of range for key 'read_timeout_seconds': expected >= 0");
		}
		if (v > static_cast<long long>(std::numeric_limits<int>::max())) {
			throw_invalid_config("value out of range for key 'read_timeout_seconds'");
		}
		result.read_timeout_seconds = static_cast<int>(v);
	}

	if (root.contains("write_timeout_seconds")) {
		const long long v = get_integer_like(root.at("write_timeout_seconds"), "write_timeout_seconds");
		if (v < 0) {
			throw_invalid_config("value out of range for key 'write_timeout_seconds': expected >= 0");
		}
		if (v > static_cast<long long>(std::numeric_limits<int>::max())) {
			throw_invalid_config("value out of range for key 'write_timeout_seconds'");
		}
		result.write_timeout_seconds = static_cast<int>(v);
	}

	if (root.contains("log_level")) {
		if (!root.at("log_level").is_string()) {
			require_type("log_level", "string");
		}
		result.log_level = root.at("log_level").get<std::string>();
	}

	if (root.contains("log_file")) {
		if (!root.at("log_file").is_string()) {
			require_type("log_file", "string");
		}
		result.log_file = root.at("log_file").get<std::string>();
	}
}

ParsedCliFlags parse_cli_flags(int argc, char** argv) {
	ParsedCliFlags flags;

	for (int i = 1; i < argc; ++i) {
		const std::string raw = argv[i] != nullptr ? argv[i] : "";
		if (raw == "-h" || raw == "--help") {
			flags.show_help = true;
			break;
		}
		if (raw == "-V" || raw == "--version") {
			flags.show_version = true;
			continue;
		}

		if (raw.rfind("--", 0) != 0) {
			throw_invalid_config("unknown argument: " + raw);
		}

		const auto split = split_flag(raw);
		const std::string& flag = split.first;
		const std::optional<std::string>& inline_value = split.second;

		if (flag == "--config") {
			flags.config_path = require_flag_value(argc, argv, i, flag, inline_value);
		} else if (flag == "--host") {
			flags.host = require_flag_value(argc, argv, i, flag, inline_value);
		} else if (flag == "--port") {
			flags.port = parse_int_value(require_flag_value(argc, argv, i, flag, inline_value), flag);
		} else if (flag == "--threads" || flag == "--base-threads") {
			flags.base_threads = parse_int_value(require_flag_value(argc, argv, i, flag, inline_value), flag);
		} else if (flag == "--max-threads") {
			flags.max_threads = parse_int_value(require_flag_value(argc, argv, i, flag, inline_value), flag);
		} else if (flag == "--max-queued") {
			flags.max_queued = parse_int_value(require_flag_value(argc, argv, i, flag, inline_value), flag);
		} else if (flag == "--max-body") {
			flags.max_body_bytes = parse_size_t_value(require_flag_value(argc, argv, i, flag, inline_value), flag);
		} else if (flag == "--log-level") {
			flags.log_level = require_flag_value(argc, argv, i, flag, inline_value);
		} else if (flag == "--log-file") {
			flags.log_file = require_flag_value(argc, argv, i, flag, inline_value);
		} else if (flag == "--read-timeout") {
			flags.read_timeout_seconds = parse_int_value(require_flag_value(argc, argv, i, flag, inline_value), flag);
		} else if (flag == "--write-timeout") {
			flags.write_timeout_seconds = parse_int_value(require_flag_value(argc, argv, i, flag, inline_value), flag);
		} else {
			throw_invalid_config("unknown flag: " + flag);
		}
	}

	return flags;
}

void apply_parsed_flags(ServerConfig& target, const ParsedCliFlags& flags) {
	if (flags.host.has_value()) {
		target.host = *flags.host;
	}
	if (flags.port.has_value()) {
		target.port = *flags.port;
	}
	if (flags.base_threads.has_value()) {
		target.base_threads = *flags.base_threads;
	}
	if (flags.max_threads.has_value()) {
		target.max_threads = *flags.max_threads;
	}
	if (flags.max_queued.has_value()) {
		target.max_queued = *flags.max_queued;
	}
	if (flags.max_body_bytes.has_value()) {
		target.max_body_bytes = *flags.max_body_bytes;
	}
	if (flags.read_timeout_seconds.has_value()) {
		target.read_timeout_seconds = *flags.read_timeout_seconds;
	}
	if (flags.write_timeout_seconds.has_value()) {
		target.write_timeout_seconds = *flags.write_timeout_seconds;
	}
	if (flags.log_level.has_value()) {
		target.log_level = *flags.log_level;
	}
	if (flags.log_file.has_value()) {
		target.log_file = *flags.log_file;
	}
	if (flags.config_path.has_value()) {
		target.config_path = *flags.config_path;
	}
}

}  // namespace

ServerConfig load_config_from_file(const std::string& path, bool required) {
	if (path.empty()) {
		return ServerConfig{};
	}

	std::error_code ec;
	if (!std::filesystem::exists(path, ec) || ec) {
		if (required) {
			throw_invalid_config("config file not found: " + path);
		}
		return ServerConfig{};
	}

	std::ifstream input(path);
	if (!input) {
		if (required) {
			throw_invalid_config("config file not found: " + path);
		}
		return ServerConfig{};
	}

	const nlohmann::json root = nlohmann::json::parse(input, nullptr, false);
	if (root.is_discarded()) {
		throw_invalid_config("invalid JSON in config: " + path);
	}
	if (!root.is_object()) {
		throw_invalid_config("invalid config root: expected JSON object");
	}

	ServerConfig result;
	validate_and_apply_config_json(result, root);
	result.config_path = path;
	return result;
}

CliParseResult parse_cli(int argc, char** argv) {
	CliParseResult result;
	const ParsedCliFlags flags = parse_cli_flags(argc, argv);
	result.show_help = flags.show_help;
	result.show_version = flags.show_version;
	if (flags.config_path.has_value()) {
		result.config_path_arg = *flags.config_path;
	}

	apply_parsed_flags(result.config, flags);
	return result;
}

void apply_cli_overrides(ServerConfig& target, int argc, char** argv) {
	const ParsedCliFlags flags = parse_cli_flags(argc, argv);
	apply_parsed_flags(target, flags);
}

void print_help(std::ostream& os, const char* argv0) {
	const ServerConfig defaults;
	os << "Usage: " << (argv0 != nullptr ? argv0 : "xmljson-service") << " [options]\n"
	   << "\n"
	   << "Options:\n"
	   << "  --config PATH           Path to JSON config file\n"
	   << "  --host HOST             Bind host (default: " << defaults.host << ")\n"
	   << "  --port N                Bind port [0..65535] (default: " << defaults.port << ")\n"
	   << "  --threads N             Alias for --base-threads\n"
	   << "  --base-threads N        Base worker thread count >= 1 (default: " << defaults.base_threads << ")\n"
	   << "  --max-threads N         Max worker threads >= 0 (default: " << defaults.max_threads << ")\n"
	   << "  --max-queued N          Max queued requests >= 0 (default: " << defaults.max_queued << ")\n"
	   << "  --max-body BYTES        Max request body bytes >= 0 (default: " << defaults.max_body_bytes << ")\n"
	   << "  --read-timeout SECS     Read timeout seconds >= 0 (default: " << defaults.read_timeout_seconds << ")\n"
	   << "  --write-timeout SECS    Write timeout seconds >= 0 (default: " << defaults.write_timeout_seconds << ")\n"
	   << "  --log-level LEVEL       Log level string (default: " << defaults.log_level << ")\n"
	   << "  --log-file PATH         Log file path; empty means stderr only (default: empty)\n"
	   << "  --help, -h              Show this help text\n"
	   << "  --version, -V           Show version and exit\n";
}

void print_version(std::ostream& os) {
	os << "xmljson-service 0.1.0\n";
}

}  // namespace xmljson