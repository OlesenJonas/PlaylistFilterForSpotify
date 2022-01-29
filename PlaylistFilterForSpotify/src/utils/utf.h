#pragma once

#include <string>

#ifdef _WIN32

    #include <windows.h>

// todo: may need the same fix as utf8_decode for not writing the null terminator into the std::string buffer
template <typename SubstrType>
std::string utf8_encode(const SubstrType& wstr)
{
    if(wstr.empty())
        return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], -1, NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], -1, &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Convert an UTF8 string to a wide Unicode String
template <typename SubstrType>
std::wstring utf8_decode(const SubstrType& str)
{
    if(str.empty())
        return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), -1, NULL, 0);
    // -1 to exclude terminator from string buffer, this may mean MultiByteToWideChar will still write a
    // terminator outside of the bounds of the "visible" buffer so probably needs bug fixing
    std::wstring wstrTo(size_needed - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], size_needed, &wstrTo[0], size_needed);
    return wstrTo;
}

#else

UTF EN - / DECODING IMPLEMENTATION MISSING FOR OTHER PLATFORMS

#endif