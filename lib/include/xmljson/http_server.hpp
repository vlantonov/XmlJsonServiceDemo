#pragma once
#include <atomic>
#include <memory>
#include <string>
#include "xmljson/config.hpp"
#include "xmljson/converter.hpp"

namespace httplib { class Server; }  // forward-declare to keep <httplib.h> out of public header

namespace xmljson {

/// \brief An HTTP server wrapping cpp-httplib, exposing endpoints for XML/JSON conversion.
class HttpServer {
 public:
    /// \brief Constructs the server. The provided converter must outlive the server instance.
    HttpServer(ServerConfig config, const Converter& converter);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    /// \brief Blocks and serves requests until stop() is called. Returns false if binding fails.
    bool listen_blocking();

    /// \brief Starts the server on a background thread. Returns false if binding fails.
    bool start_background();

    /// \brief Stops the server and joins the background thread if started in the background.
    void stop();

    /// \brief Returns true if the server is currently running.
    bool is_running() const noexcept;

    /// \brief Returns the actual bound port, or 0 if not running.
    int  bound_port() const noexcept;

    /// \brief Returns the server's configuration instance.
    const ServerConfig& config() const noexcept;

 private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace xmljson
