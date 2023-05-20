#pragma once

#include <exception>

class UnknownApiError : public std::exception
{
    const char* what() const noexcept final;
};