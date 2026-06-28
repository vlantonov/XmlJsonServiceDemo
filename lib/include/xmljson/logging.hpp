#pragma once
#include <memory>
#include <string>
#include <spdlog/spdlog.h>

namespace xmljson {

/// \brief Configuration for the "xmljson" logger.
struct LoggingConfig {
    std::string level    = "info";  // parsed by spdlog::level::from_str
    std::string log_file = "";       // empty = stderr only
};

/// \brief Initializes the global default logger and creates the "xmljson" named logger.
void init_logging(const LoggingConfig& cfg);

/// \brief Returns the "xmljson" logger, creating it with defaults if not already initialized.
std::shared_ptr<spdlog::logger> logger();

/// \brief Parses a string into a spdlog level. Throws ConversionError(InvalidConfig) on unknown.
spdlog::level::level_enum parse_level(const std::string& s);

}  // namespace xmljson
