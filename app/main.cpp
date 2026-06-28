#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "xmljson/config.hpp"
#include "xmljson/converter.hpp"
#include "xmljson/error.hpp"
#include "xmljson/http_server.hpp"
#include "xmljson/logging.hpp"

namespace {

static xmljson::HttpServer* g_server = nullptr;

extern "C" void on_signal(int) {
	if (g_server != nullptr) {
		g_server->stop();
	}
}

}  // namespace

namespace xmljson {
void apply_cli_overrides(ServerConfig&, int, const char* const*);
}

int main(int argc, char** argv) {
	try {
		xmljson::CliParseResult cli;
		try {
			cli = xmljson::parse_cli(argc, argv);
		} catch (const xmljson::ConversionError& e) {
			if (e.code() == xmljson::ErrorCode::InvalidConfig) {
				std::cerr << e.what() << "\n";
				xmljson::print_help(std::cerr, argc > 0 ? argv[0] : "xmljson-service");
				return 2;  // CLI / config validation error
			}
			throw;
		}

		if (cli.show_help) {
			xmljson::print_help(std::cout, argc > 0 ? argv[0] : "xmljson-service");
			return 0;  // clean exit
		}

		if (cli.show_version) {
			xmljson::print_version(std::cout);
			return 0;  // clean exit
		}

		const bool config_required = !cli.config_path_arg.empty();
		xmljson::ServerConfig cfg = xmljson::load_config_from_file(cli.config_path_arg, config_required);
		xmljson::apply_cli_overrides(cfg, argc, argv);

		xmljson::init_logging({cfg.log_level, cfg.log_file});

		xmljson::Converter converter;
		xmljson::HttpServer server(cfg, converter);

		g_server = &server;
		std::signal(SIGINT, on_signal);
#ifndef _WIN32
		std::signal(SIGTERM, on_signal);
#endif

		xmljson::logger()->info(
			"starting xmljson-service host={} port={} base_threads={} max_threads={} max_queued={} max_body_bytes={} "
			"read_timeout_seconds={} write_timeout_seconds={}",
			cfg.host,
			cfg.port,
			cfg.base_threads,
			cfg.max_threads,
			cfg.max_queued,
			cfg.max_body_bytes,
			cfg.read_timeout_seconds,
			cfg.write_timeout_seconds);

		if (!server.listen_blocking()) {
			xmljson::logger()->error("failed to bind xmljson-service on {}:{}", cfg.host, cfg.port);
			g_server = nullptr;
			return 3;  // failed to bind socket
		}

		xmljson::logger()->info("xmljson-service stopped");
		g_server = nullptr;
		return 0;  // clean exit
	} catch (const xmljson::ConversionError& e) {
		std::cerr << "error: " << e.what() << "\n";
		return 2;  // CLI / config validation error
	} catch (const std::exception& e) {
		std::cerr << "fatal: " << e.what() << "\n";
		return 1;  // uncaught exception
	}
}