#include "xmljson/http_server.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "xmljson/error.hpp"
#include "xmljson/logging.hpp"

namespace xmljson {

namespace {

std::string to_lower_ascii(std::string value) {
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return value;
}

std::string trim_ascii(std::string value) {
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
		value.erase(value.begin());
	}
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
		value.pop_back();
	}
	return value;
}

bool ends_with(std::string_view value, std::string_view suffix) {
	return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

std::string content_type_without_params(const httplib::Request& req) {
	const std::string raw = req.get_header_value("Content-Type");
	const std::size_t semi = raw.find(';');
	const std::string media = semi == std::string::npos ? raw : raw.substr(0, semi);
	return to_lower_ascii(trim_ascii(media));
}

bool is_xml_content_type(std::string_view media_type) {
	return media_type == "application/xml" || media_type == "text/xml" || ends_with(media_type, "+xml");
}

bool is_json_content_type(std::string_view media_type) {
	return media_type == "application/json" || ends_with(media_type, "+json");
}

enum class ConvertDirection {
	XmlToJson,
	JsonToXml,
	Unsupported,
};

ConvertDirection detect_convert_direction(const httplib::Request& req) {
	const std::string media = content_type_without_params(req);
	if (is_xml_content_type(media)) {
		return ConvertDirection::XmlToJson;
	}
	if (is_json_content_type(media)) {
		return ConvertDirection::JsonToXml;
	}
	return ConvertDirection::Unsupported;
}

std::string status_error_name(int status) {
	switch (status) {
		case 404:
			return "NotFound";
		case 413:
			return to_string(ErrorCode::PayloadTooLarge);
		case 415:
			return to_string(ErrorCode::UnsupportedMediaType);
		case 500:
			return "InternalError";
		default:
			return std::to_string(status);
	}
}

std::string status_message(int status) {
	switch (status) {
		case 400:
			return "bad request";
		case 404:
			return "resource not found";
		case 413:
			return "payload too large";
		case 415:
			return "unsupported media type";
		case 500:
			return "internal server error";
		default:
			return "http error";
	}
}

void write_error_json(httplib::Response& res, int status, const std::string& error_name,
					  const std::string& message, const std::string& path) {
	nlohmann::json envelope = {
		{"error", error_name},
		{"message", message},
		{"path", path},
	};
	res.status = status;
	res.set_content(envelope.dump(), "application/json");
}

}  // namespace

struct HttpServer::Impl {
	ServerConfig config;
	const Converter& converter;
	httplib::Server svr;
	std::thread worker;
	// Shared lifecycle state crossing threads is synchronized via atomics.
	std::atomic<bool> running{false};
	std::atomic<int> bound_port{0};
	std::atomic<int> startup_state{0};

	Impl(ServerConfig cfg, const Converter& conv) : config(std::move(cfg)), converter(conv) {}

	void install_routes();
	void install_handlers();
};

void HttpServer::Impl::install_handlers() {
	svr.set_payload_max_length(config.max_body_bytes);
	svr.set_read_timeout(config.read_timeout_seconds, 0);
	svr.set_write_timeout(config.write_timeout_seconds, 0);

	const int base_threads = config.base_threads > 0 ? config.base_threads : 1;
	const int max_queued = config.max_queued > 0 ? config.max_queued : 0;
	svr.new_task_queue = [base_threads, max_queued]() {
		return new httplib::ThreadPool(static_cast<std::size_t>(base_threads),
									   static_cast<std::size_t>(max_queued));
	};

	svr.set_error_handler([this](const httplib::Request& req, httplib::Response& res) {
		if (!res.body.empty()) {
			return;
		}
		const int status = res.status > 0 ? res.status : 500;
		write_error_json(res, status, status_error_name(status), status_message(status), req.path);
	});

	svr.set_exception_handler([this](const httplib::Request& req, httplib::Response& res, std::exception_ptr ep) {
		try {
			if (ep) {
				std::rethrow_exception(ep);
			}
			write_error_json(res, 500, "InternalError", "internal server error", req.path);
			logger()->error("HTTP {} {} failed with unknown non-std exception", req.method, req.path);
		} catch (const ConversionError& ex) {
			int status = 400;
			switch (ex.code()) {
				case ErrorCode::MalformedInput:
				case ErrorCode::UnsupportedShape:
					status = 400;
					break;
				case ErrorCode::UnsupportedMediaType:
					status = 415;
					break;
				case ErrorCode::PayloadTooLarge:
					status = 413;
					break;
				default:
					status = 500;
					break;
			}

			const std::string error_name = status == 500 ? "InternalError" : to_string(ex.code());
			write_error_json(res, status, error_name, ex.what(), req.path);
			if (status >= 500) {
				logger()->error("HTTP {} {} failed: {}", req.method, req.path, ex.what());
			} else {
				logger()->warn("HTTP {} {} rejected ({}): {}", req.method, req.path, error_name, ex.what());
			}
		} catch (const std::exception& ex) {
			write_error_json(res, 500, "InternalError", ex.what(), req.path);
			logger()->error("HTTP {} {} failed with unhandled exception: {}", req.method, req.path, ex.what());
		} catch (...) {
			write_error_json(res, 500, "InternalError", "internal server error", req.path);
			logger()->error("HTTP {} {} failed with unknown non-std exception", req.method, req.path);
		}
	});
}

void HttpServer::Impl::install_routes() {
	svr.Post("/convert", [this](const httplib::Request& req, httplib::Response& res) {
		const ConvertDirection direction = detect_convert_direction(req);
		if (direction == ConvertDirection::Unsupported) {
			throw ConversionError(ErrorCode::UnsupportedMediaType,
								  "Content-Type must be XML or JSON for /convert");
		}

		if (direction == ConvertDirection::XmlToJson) {
			const std::string output = converter.xml_to_json(req.body);
			res.set_content(output, "application/json");
			return;
		}

		const std::string output = converter.json_to_xml(req.body);
		res.set_content(output, "application/xml; charset=utf-8");
	});

	svr.Post("/xml-to-json", [this](const httplib::Request& req, httplib::Response& res) {
		const std::string output = converter.xml_to_json(req.body);
		res.set_content(output, "application/json");
	});

	svr.Post("/json-to-xml", [this](const httplib::Request& req, httplib::Response& res) {
		const std::string output = converter.json_to_xml(req.body);
		res.set_content(output, "application/xml; charset=utf-8");
	});

	svr.Get("/healthz", [](const httplib::Request&, httplib::Response& res) {
		nlohmann::json body = {{"status", "ok"}};
		res.set_content(body.dump(), "application/json");
	});

	svr.Get("/version", [](const httplib::Request&, httplib::Response& res) {
		nlohmann::json body = {{"name", "xmljson"}, {"version", "0.1.0"}};
		res.set_content(body.dump(), "application/json");
	});
}

HttpServer::HttpServer(ServerConfig config, const Converter& converter)
	: impl_(std::make_unique<Impl>(std::move(config), converter)) {
	impl_->install_handlers();
	impl_->install_routes();
}

HttpServer::~HttpServer() {
	stop();
}

bool HttpServer::listen_blocking() {
	impl_->startup_state.store(0);
	impl_->bound_port.store(0);

	if (impl_->config.port == 0) {
		const int actual = impl_->svr.bind_to_any_port(impl_->config.host);
		if (actual < 0) {
			impl_->startup_state.store(-1);
			return false;
		}
		impl_->bound_port.store(actual);
	} else {
		if (!impl_->svr.bind_to_port(impl_->config.host, impl_->config.port)) {
			impl_->startup_state.store(-1);
			return false;
		}
		impl_->bound_port.store(impl_->config.port);
	}

	impl_->startup_state.store(1);
	impl_->running.store(true);
	const bool ok = impl_->svr.listen_after_bind();
	impl_->running.store(false);
	impl_->bound_port.store(0);
	return ok;
}

bool HttpServer::start_background() {
	if (impl_->worker.joinable()) {
		return impl_->svr.is_running();
	}

	impl_->worker = std::thread([this]() {
		(void)listen_blocking();
	});

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < deadline) {
		if (impl_->startup_state.load() < 0) {
			if (impl_->worker.joinable()) {
				impl_->worker.join();
			}
			return false;
		}

		if (impl_->svr.is_running()) {
			return true;
		}

		if (impl_->startup_state.load() > 0 && !impl_->running.load()) {
			if (impl_->worker.joinable()) {
				impl_->worker.join();
			}
			return false;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	if (impl_->svr.is_running()) {
		return true;
	}

	stop();
	return false;
}

void HttpServer::stop() {
	impl_->svr.stop();
	if (impl_->worker.joinable()) {
		impl_->worker.join();
	}
	impl_->running.store(false);
	impl_->bound_port.store(0);
}

bool HttpServer::is_running() const noexcept {
	return impl_->svr.is_running();
}

int HttpServer::bound_port() const noexcept {
	if (!impl_->svr.is_running()) {
		return 0;
	}
	return impl_->bound_port.load();
}

const ServerConfig& HttpServer::config() const noexcept {
	return impl_->config;
}

}  // namespace xmljson