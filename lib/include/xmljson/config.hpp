#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <iosfwd>

namespace xmljson {

/// \brief Server configuration settings.
struct ServerConfig {
    std::string host             = "0.0.0.0";
    int         port             = 8080;
    int         base_threads     = 8;   // initial worker count
    int         max_threads      = 0;   // 0 = unbounded (cpp-httplib uses base for both when 0)
    int         max_queued       = 0;
    std::size_t max_body_bytes   = 16 * 1024 * 1024;  // 16 MiB
    int         read_timeout_seconds  = 30;
    int         write_timeout_seconds = 30;
    std::string log_level        = "info";  // trace|debug|info|warn|error|critical|off
    std::string log_file         = "";       // empty = stderr only
    std::string config_path      = "";       // path the config was loaded from, for logging
};

/// \brief Loads configuration from a JSON file. Throws ConversionError(InvalidConfig) on failure.
ServerConfig load_config_from_file(const std::string& path, bool required = false);

/// \brief Result of parsing command-line arguments.
struct CliParseResult {
    ServerConfig config;
    bool show_help    = false;
    bool show_version = false;
    std::string config_path_arg; // value of --config if provided
};

/// \brief Parses CLI arguments into a partial config. Does not load the config file.
CliParseResult parse_cli(int argc, const char* const* argv);

/// \brief Applies CLI overrides to an existing config without loading a config file.
void apply_cli_overrides(ServerConfig& target, int argc, const char* const* argv);

/// \brief Prints usage help to the specified output stream.
void print_help(std::ostream& os, const char* argv0);

/// \brief Prints version text to the specified output stream.
void print_version(std::ostream& os);

}  // namespace xmljson
