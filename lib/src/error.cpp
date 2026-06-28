#include "xmljson/error.hpp"

#include <utility>

namespace xmljson {

const char* to_string(const ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::MalformedInput:
            return "MalformedInput";
        case ErrorCode::UnsupportedShape:
            return "UnsupportedShape";
        case ErrorCode::EncodingError:
            return "EncodingError";
        case ErrorCode::PayloadTooLarge:
            return "PayloadTooLarge";
        case ErrorCode::UnsupportedMediaType:
            return "UnsupportedMediaType";
        case ErrorCode::InvalidConfig:
            return "InvalidConfig";
    }
    return "Unknown";
}

ConversionError::ConversionError(const ErrorCode code, std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

ErrorCode ConversionError::code() const noexcept {
    return code_;
}

}  // namespace xmljson