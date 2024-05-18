#pragma once
#include <string>
#include <string_view>
#include <stringapiset.h>
#include <wil/stl.h>
#include <wil/result_macros.h>
#include <wil/win32_helpers.h>
#include <wil/stl.h>
#include <wil/filesystem.h>

inline std::wstring from_utf8(std::string_view text)
{
    if (text.length() == 0)
    {
        return std::wstring{};
    }

    int bufferSize{MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.length()), nullptr, 0)};

    FAIL_FAST_IF(bufferSize == 0); // Should not occur, but do not ignore unexpected errors.

    std::wstring buffer(bufferSize, '\0');

    int written{MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.length()), &buffer[0], bufferSize)};
    THROW_LAST_ERROR_IF(written != bufferSize);

    return buffer;
}

// return a string_view into the buffer that will skip the BOM if needed.
inline std::string_view skip_utf8_bom(std::string_view content)
{
    constexpr std::array<unsigned char, 3> bomHeader{0xEF, 0xBB, 0xBF};

    if ((content.size() >= bomHeader.size()) && (memcmp(content.data(), bomHeader.data(), bomHeader.size()) == 0))
    {
        return content.substr(bomHeader.size()); // BOM present, skip it.
    }
    return content;
}

inline std::wstring read_utf8_file(PCWSTR path)
{
    // auto fileHandle = wil::open_file(path); // new version of wil library required for this
    auto fileHandle = wil::unique_hfile{CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    THROW_LAST_ERROR_IF(!fileHandle.is_valid());

    LARGE_INTEGER size{};
    THROW_IF_WIN32_BOOL_FALSE(GetFileSizeEx(fileHandle.get(), &size));

    DWORD fileSize = static_cast<DWORD>(size.QuadPart);
    auto bufferPtr = std::make_unique<char[]>(static_cast<size_t>(fileSize));
    THROW_IF_WIN32_BOOL_FALSE(ReadFile(fileHandle.get(), bufferPtr.get(), fileSize, nullptr, nullptr));
    return from_utf8(skip_utf8_bom({bufferPtr.get(), fileSize}));
}

// TODO: Move these to wil/win32_helpers.h. That requires adding a dependency on
// wil/stl.h and libloaderapi.h to pick up wil::zstring_view + the resource APIs.
// This may be a problem for projects that include wil/win32_helpers.h based on these
// new dependencies so this approach needs to be verified.
//
// If that is a problem it may be that including wil/stl.h is not strictly needed
// given the delayed instantiation of wil::zstring_view, referenced in templates.
// In this approach clients will need a new include libloaderapi.h. Annoying but workable.

// auto json = get_resource_string_view(ID_JSON_CONTENT);

// TODO: test using wil::zwstring_view to see if that work I suspect these are always null terminated
// and that is often needed, providing the most flexibility for the caller.
template <typename T = std::wstring_view>
std::wstring_view get_resource_string_view(UINT resourceId, HINSTANCE module = wil::GetModuleInstanceHandle())
{
    PCWSTR messageStringBuffer{};
    auto messageStringLength = LoadStringW(module, resourceId, reinterpret_cast<PWSTR>(&messageStringBuffer), 0);
    return {messageStringBuffer, static_cast<size_t>(messageStringLength)};
}

// auto json = get_resource_string_view(L"File.json");
//
// auto json = get_resource_string_view<wchar_t>(L"Unicode Content.txt");
template <typename T = char> // default to UTF-8, use wchar_t for UTF-16
std::basic_string_view<T> get_resource_view(PCWSTR name, PCWSTR type = RT_RCDATA, HINSTANCE module = wil::GetModuleInstanceHandle())
{
    if (HRSRC resourceHandle = FindResourceW(module, name, type))
    {
        if (HGLOBAL resourceData = LoadResource(module, resourceHandle))
        {
            auto resourceSize = SizeofResource(module, resourceHandle);
            return {static_cast<T*>(LockResource(resourceData)), resourceSize / sizeof(T)};
        }
    }
    return {};
}

// auto json = get_resource_string_view(ID_JSON_FILE);
template <typename T = char> // default to UTF-8, use wchar_t for UTF-16
inline std::basic_string_view<T> get_resource_view(UINT resourceId, PCWSTR type = RT_RCDATA, HINSTANCE module = wil::GetModuleInstanceHandle())
{
    return get_resource_view<T>(MAKEINTRESOURCEW(resourceId), type, module);
}
