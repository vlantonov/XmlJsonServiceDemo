#include "xmljson/logging.hpp"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "xmljson/error.hpp"

namespace xmljson {

spdlog::level::level_enum parse_level(const std::string& s) {
	std::string lowered = s;
	std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});

	static const std::vector<std::string> kKnownLevels = {
		"trace", "debug", "info", "warn", "warning", "error", "err", "critical", "off"
	};

	if (std::find(kKnownLevels.begin(), kKnownLevels.end(), lowered) == kKnownLevels.end()) {
		throw ConversionError(ErrorCode::InvalidConfig, "unknown log level: " + s);
	}

	return spdlog::level::from_str(lowered);
}

void init_logging(const LoggingConfig& cfg) {
	static std::mutex init_mutex;
	std::lock_guard<std::mutex> lock(init_mutex);

	const auto lvl = parse_level(cfg.level);

	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
	if (!cfg.log_file.empty()) {
		sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(cfg.log_file, false));
	}

	auto named_logger = std::make_shared<spdlog::logger>("xmljson", sinks.begin(), sinks.end());
	named_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
	named_logger->set_level(lvl);

	spdlog::drop("xmljson");
	spdlog::register_logger(named_logger);
	spdlog::set_default_logger(named_logger);
}

std::shared_ptr<spdlog::logger> logger() {
	auto named_logger = spdlog::get("xmljson");
	if (named_logger) {
		return named_logger;
	}

	init_logging(LoggingConfig{});
	return spdlog::get("xmljson");
}

}  // namespace xmljson