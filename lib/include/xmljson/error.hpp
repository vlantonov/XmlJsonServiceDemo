#pragma once
#include <stdexcept>
#include <string>

namespace xmljson {

/// \brief Represents different categories of conversion and server errors.
enum class ErrorCode {
    MalformedInput,       // failed to parse XML or JSON
    UnsupportedShape,     // JSON shape can't be expressed as XML
    EncodingError,        // serialization failed
    PayloadTooLarge,      // HTTP layer — body exceeds configured limit
    UnsupportedMediaType, // HTTP layer — content-type not supported
    InvalidConfig         // config loader — invalid value
};

/// \brief Returns a stable string representation of the given error code.
const char* to_string(ErrorCode code) noexcept;

/// \brief Exception thrown for conversion and configuration errors.
class ConversionError : public std::runtime_error {
 public:
    /// \brief Constructs a new ConversionError with the specified code and message.
    ConversionError(ErrorCode code, std::string message);
    
    /// \brief Returns the error code associated with this exception.
    ErrorCode code() const noexcept;
 private:
    ErrorCode code_;
};

}  // namespace xmljson
