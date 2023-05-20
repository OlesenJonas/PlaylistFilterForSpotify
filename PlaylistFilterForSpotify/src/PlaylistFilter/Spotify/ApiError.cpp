#include "ApiError.hpp"

const char* UnknownApiError::what() const noexcept
{
    return "An unknown api error has occured";
}