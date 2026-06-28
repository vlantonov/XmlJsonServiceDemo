# XmlJsonServiceDemo

XmlJsonServiceDemo is a small C++17 HTTP service that converts XML to JSON and JSON to XML, built as a clean library-plus-transport architecture with zero manual dependency setup. It is fully self-contained via CMake FetchContent, test-driven with GoogleTest (99 passing tests), and intended to build and run consistently across Linux, macOS, and Windows.

## Status
- 99 tests passing (94 unit + 5 integration)
- Build: cmake + git + a C++17 compiler. Nothing else.

## Supported platforms
- Linux (Ubuntu 22.04+ / GCC 11+ or Clang 14+) - primary
- macOS 12+ (Apple Clang or Homebrew Clang)
- Windows 10+ (MSVC 2019/2022) - supported (header-only deps, MSVC-friendly)

## Architecture
The project is split into a pure conversion library and a thin HTTP app binary.
- Core library: conversion logic and error/config/logging primitives in [lib/src](lib/src), with public headers in [lib/include/xmljson](lib/include/xmljson).
- HTTP transport: [lib/src/http_server.cpp](lib/src/http_server.cpp) wraps cpp-httplib and exposes routes.
- App entrypoint: [app/main.cpp](app/main.cpp) parses CLI/config, initializes logging, and runs the server.

Threading model:
- Request handling uses cpp-httplib's worker-pool model (`ThreadPool`) sized by `base_threads` and bounded by `max_queued`.
- Shared lifecycle state in `HttpServer` (`running`, `bound_port`, startup state) is synchronized via atomics.

## Dependencies (fetched automatically by CMake)
| Library | Version | Purpose |
|---|---|---|
| cpp-httplib | v0.16.0 | HTTP server (header-only, bounded thread pool) |
| nlohmann/json | v3.11.3 | JSON parsing & emission |
| pugixml | v1.14 | XML parsing & emission |
| spdlog | v1.14.1 | Structured logging |
| GoogleTest | v1.14.0 | Unit & integration tests |

## Conversion semantics
The converter uses the following Parker-style mapping conventions:
- `<a/> -> null`
- Text-only element -> bare JSON string
- XML attributes are emitted with `@` prefix by default (`attribute_prefix`, configurable)
- Mixed content text is concatenated under `#text` (`text_key`, configurable)
- Limitation: ordering between text segments and child elements is not preserved
- Repeated sibling elements become JSON arrays
- Single child occurrences are not arrays unless `force_array_keys=true` or the key is in `always_array_keys`
- XML namespaces are preserved verbatim in element/attribute names
- JSON->XML requires a single-keyed root object whose root value is not an array
- Nested arrays in JSON are rejected (`UnsupportedShape`)
- JSON numbers, booleans, and null are emitted as XML text (`null` becomes empty text)

Example XML -> JSON:
```xml
<note priority="high"><to>Alice</to><body>Hello</body><empty/></note>
```
```json
{"note":{"@priority":"high","to":"Alice","body":"Hello","empty":null}}
```

Example JSON -> XML:
```json
{"note":{"@priority":"high","to":"Bob","body":"Hello","empty":null}}
```
```xml
<?xml version="1.0" encoding="UTF-8"?><note priority="high"><to>Bob</to><body>Hello</body><empty/></note>
```

## HTTP API
| Method | Path | Request Content-Type | Response Content-Type | Behavior |
|---|---|---|---|---|
| POST | /convert | application/xml or text/xml or *+xml | application/json | XML -> JSON |
| POST | /convert | application/json or *+json | application/xml; charset=utf-8 | JSON -> XML |
| POST | /xml-to-json | (any) | application/json | Force XML -> JSON |
| POST | /json-to-xml | (any) | application/xml; charset=utf-8 | Force JSON -> XML |
| GET | /healthz | - | application/json | `{"status":"ok"}` |
| GET | /version | - | application/json | `{"name":"xmljson","version":"0.1.0"}` |

Error envelope:
```json
{ "error": "MalformedInput", "message": "...", "path": "/convert" }
```

| Code | Meaning |
|---|---|
| 400 | MalformedInput, UnsupportedShape |
| 404 | NotFound |
| 413 | PayloadTooLarge |
| 415 | UnsupportedMediaType |
| 500 | InternalError |

## Build
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

First configure downloads all dependencies via FetchContent (typically 5-10 minutes on a cold cache). Subsequent configures reuse [build/_deps](build/_deps).

### CMake options
| Option | Default | Description |
|---|---|---|
| `XMLJSON_BUILD_TESTS` | `ON` | Build unit & integration tests |
| `XMLJSON_BUILD_APP` | `ON` | Build the `xmljson-service` binary |
| `XMLJSON_ENABLE_COVERAGE` | `OFF` | Enable lcov-based coverage target |
| `XMLJSON_ENABLE_ASAN` | `OFF` | Enable AddressSanitizer |
| `XMLJSON_ENABLE_UBSAN` | `OFF` | Enable UndefinedBehaviorSanitizer |
| `XMLJSON_WARNINGS_AS_ERRORS` | `OFF` | Promote warnings to errors |

## Run
```sh
./build/xmljson-service --port 8080 --threads 8
```

Or with a config file:
```sh
./build/xmljson-service --config config/default.json
```

CLI flags override values from the config file.

### CLI flags
```text
Usage: ./build/xmljson-service [options]

Options:
	--config PATH           Path to JSON config file
	--host HOST             Bind host (default: 0.0.0.0)
	--port N                Bind port [0..65535] (default: 8080)
	--threads N             Alias for --base-threads
	--base-threads N        Base worker thread count >= 1 (default: 8)
	--max-threads N         Max worker threads >= 0 (default: 0)
	--max-queued N          Max queued requests >= 0 (default: 0)
	--max-body BYTES        Max request body bytes >= 0 (default: 16777216)
	--read-timeout SECS     Read timeout seconds >= 0 (default: 30)
	--write-timeout SECS    Write timeout seconds >= 0 (default: 30)
	--log-level LEVEL       Log level string (default: info)
	--log-file PATH         Log file path; empty means stderr only (default: empty)
	--help, -h              Show this help text
	--version, -V           Show version and exit
```

Version output:
```text
xmljson-service 0.1.0
```

### Sample config (config/default.json)
```json
{
	"host": "0.0.0.0",
	"port": 8080,
	"base_threads": 8,
	"max_threads": 0,
	"max_queued": 0,
	"max_body_bytes": 16777216,
	"read_timeout_seconds": 30,
	"write_timeout_seconds": 30,
	"log_level": "info",
	"log_file": ""
}
```

### Example requests
```sh
curl -X POST -H 'Content-Type: application/xml' \
		 --data '<note><to>Alice</to><body>Hi</body></note>' \
		 http://localhost:8080/convert
# -> {"note":{"to":"Alice","body":"Hi"}}

curl -X POST -H 'Content-Type: application/json' \
		 --data '{"note":{"to":"Bob","body":"Hello"}}' \
		 http://localhost:8080/convert
# -> <?xml version="1.0" encoding="UTF-8"?><note><to>Bob</to><body>Hello</body></note>

curl http://localhost:8080/healthz
# -> {"status":"ok"}
```

## Tests
```sh
cd build
ctest --output-on-failure                   # all tests (99 total)
ctest --output-on-failure -LE integration   # unit only (94)
ctest --output-on-failure -L integration    # integration only (5)
```

### Coverage (Linux/macOS)
```sh
cmake -S . -B build-cov -DXMLJSON_ENABLE_COVERAGE=ON
cmake --build build-cov -j
cmake --build build-cov --target cov
# Open build-cov/cov/index.html
```

Requires `lcov` and `genhtml` on PATH.

### Sanitizers (Linux/macOS)
```sh
cmake -S . -B build-asan -DXMLJSON_ENABLE_ASAN=ON
cmake --build build-asan -j
cd build-asan && ctest --output-on-failure
```

## Project layout
```text
.
|- CMakeLists.txt
|- README.md
|- LICENSE
|- app/
|  \- main.cpp
|- cmake/
|  |- Dependencies.cmake
|  |- CompileOptions.cmake
|  \- CodeCoverage.cmake
|- config/
|  \- default.json
|- lib/
|  |- include/xmljson/
|  |  |- config.hpp
|  |  |- converter.hpp
|  |  |- conversion_options.hpp
|  |  \- http_server.hpp
|  \- src/
|     |- config.cpp
|     |- converter.cpp
|     |- http_server.cpp
|     |- json_to_xml.cpp
|     |- logging.cpp
|     \- xml_to_json.cpp
|- tests/
|  |- main.cpp
|  |- test_config.cpp
|  |- test_http_handlers.cpp
|  |- test_json_to_xml.cpp
|  |- test_logging.cpp
|  |- test_round_trip.cpp
|  |- test_xml_to_json.cpp
|  |- fixtures/
|  \- integration/
|     \- test_http_server.cpp
\- build/                 # generated
```

## Scalability notes
- Transport scalability uses cpp-httplib's worker-pool model, tuned via `base_threads` and `max_queued` in config/CLI.
- Functional scalability is route-oriented: new endpoints are installed in `HttpServer::install_routes` in [lib/src/http_server.cpp](lib/src/http_server.cpp).
- The `HttpServer` pimpl keeps cpp-httplib out of the public header ([lib/include/xmljson/http_server.hpp](lib/include/xmljson/http_server.hpp)), making backend swaps (for example, Boost.Beast) possible without changing consumers.

## Daemon mode
Not implemented in v1 - rely on `systemd`, `nohup`, or a Windows service wrapper. Graceful shutdown is supported via SIGINT/SIGTERM.

## Task description
Create a web service (over TCP/IP). the service allows to convert XML to JSON and vice versa.

Supported requests:
* convert the source data

Application specification:
* communication: any convenient protocol (HTTP, JSON-prc, SOAP, etc.)
* async server architecture
* third party framewroks and libraries are preferable
* C++11 is allowed
* focus on vertical and functional scalability
* no GUI
* daemon (service) mode, optional
* configuration via config file, optional
* logging (any solution)
* unit tests (any suitable framework) required

Estimate work before realization.

Desired outcome:
* instructions with information on supported platforms and build tests
* source code
* tests

## License
Licensed under the terms in [LICENSE](LICENSE).