#include "ApiError.h"

const char* UnknownApiError::what() const noexcept
{
    return "An unknown api error has occured";
}