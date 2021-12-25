#pragma once
#include <winuser.h>
#include <shellscalingapi.h>
#include <string>
#include <cassert>
#include <wil/common.h>

// This tells the linker to generate the manifest entires that use comclt32 v6.
// This makes Win32 controls based UIs look less bad.
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

inline std::wstring GetWindowTextAlloc(HWND window)
{
    const auto length = GetWindowTextLengthW(window) + 1; // with space for null
    std::wstring result(length, L'\0');
    const auto retrievedSize = GetWindowTextW(window, result.data(), static_cast<int>(result.capacity()));
    result.resize(result.size() - 1); // trim null terminator
    assert(result.size() == wcslen(result.c_str()));
    return result;
}

inline std::wstring GetWindowClassAlloc(HWND window)
{
    std::wstring result(64, L'\0');
    result.resize(GetClassNameW(window, result.data(), static_cast<int>(result.size())));
    return result;
}

inline void EnableProcessDefaults(PCWSTR mainThreadName = L"Main Thread")
{
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);   // crash quickly to find heap overwrite bugs
    SetThreadDescription(GetCurrentThread(), mainThreadName);
}

// Enumerate each top level window in the system (there will be a lot).
template <typename TCallback>
bool ForEachTopLevelWindow(TCallback&& callback)
{
    return EnumWindows(
        [](HWND data, LPARAM lParam) -> BOOL { return (*reinterpret_cast<const TCallback*>(lParam))(data); },
        reinterpret_cast<LPARAM>(&callback)) != FALSE;
}

inline bool IsAppLikeWindow(HWND window, HWND filterWindow = nullptr)
{
    if (window == filterWindow)
    {
        return false;
    }

    const auto style = GetWindowLongW(window, GWL_STYLE);
    const auto exStyle = GetWindowLongW(window, GWL_EXSTYLE);

    // This expression implements the policy of skipping windows unless they are visible,
    // not minimized, not tool windows, can take activation and have non-zero size.
    RECT windowRect{};
    return WI_IsFlagSet(style, WS_VISIBLE) &&
        WI_IsFlagClear(style, WS_MINIMIZE) &&
        WI_AreAllFlagsClear(exStyle, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE) &&
        (GetWindowRect(window, &windowRect) != FALSE) &&
        (windowRect.left != windowRect.right) && (windowRect.top != windowRect.bottom) &&
        (GetWindow(window, GW_OWNER) == nullptr);
}

inline void ReplaceWindowIcon(HWND hwnd, BOOL fSmall, HICON hIcon)
{
    hIcon = (HICON)SendMessageW(hwnd, WM_SETICON, fSmall ? ICON_SMALL : ICON_BIG, (LPARAM)hIcon);
    if (hIcon)
        DestroyIcon(hIcon);
}

