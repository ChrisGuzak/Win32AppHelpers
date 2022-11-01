# Win32 App Helpers for C++

Helpers to make writing Win32 GUI style applications simpler. They include the ability to create
top level windows, run the message pump, and handle message concisely, by implementing functions
with specific names on the class that represents the window.

A 56 line Win32 App

```cpp
#include "pch.h"
#include <win32app/win32_app_helpers.h>

struct AppWindow
{
    wil::unique_hwnd m_window;

    void Show(int nCmdShow)
    {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken;
        Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

        win32app::create_top_level_window(*this, L"Win32XamlDialogWindow");
        const auto dpi = GetDpiForWindow(m_window.get());
        const int dx = (600 * dpi) / 96;
        const int dy = (800 * dpi) / 96;
        SetWindowPos(m_window.get(), nullptr, 0, 0, dx, dy, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
        win32app::enter_simple_message_loop(*this, nCmdShow);
    }

    LRESULT Destroy()
    {
        PostQuitMessage(0); // exit message loop
        return 0;
    }

    LRESULT Paint(HDC hdc, const PAINTSTRUCT& ps)
    {
        Gdiplus::Graphics graphics(hdc);

        const auto dpi = GetDpiForWindow(m_window.get());

        const auto emSize = static_cast<float>((12.0 * dpi) / 96); // 24 pt
        Gdiplus::Font font(L"Segoe UI", emSize, Gdiplus::FontStyleRegular);

        Gdiplus::SolidBrush brush(Gdiplus::Color(255, 0, 100, 255));

        const float drawOffset = 100; // view pixels
        const float dpiScaledX = (drawOffset * dpi) / 96;
        const float dpiScaledY = (drawOffset * dpi) / 96;

        const wchar_t message[]{ L"Hello from GDIPlus!" };
        graphics.DrawString(message, static_cast<int>(wcslen(message)), &font, { dpiScaledX, dpiScaledY }, &brush);
        return 0;
    }
};

_Use_decl_annotations_ int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR lpCmdLine, int nCmdShow)
{
    auto coInit = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
    std::make_unique<AppWindow>()->Show(nCmdShow);
    return 0;
}
```

Simple App (20 lines)

```cpp
struct SimplestAppWindow
{
    wil::unique_hwnd m_window;

    void Show(int nCmdShow)
    {
        win32app::create_top_level_window(*this, L"Win32XamlDialogWindow");
        win32app::enter_simple_message_loop(*this, nCmdShow);
    }

    LRESULT Destroy()
    {
        PostQuitMessage(0); // exit message loop
        return 0;
    }
};

_Use_decl_annotations_ int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR lpCmdLine, int nCmdShow)
{
    std::make_unique<SimplestAppWindow>()->Show(nCmdShow);
}
```

### Consuming the helpers

There are 2 methods for consuming Win32AppHelpers.

#### Using a git submodule

Add `Win32AppHelpers` as a sub-module to your repository.

```
git submodule add https://github.com/ChrisGuzak/Win32AppHelpers.git
```

Hand edit the .vcxproj file to reference `Win32AppHelpers\inc\include_this_dir.targets` just below the 
import of `Microsoft.Cpp.Default.props`, using the relative path to the submodule location.

```xml
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <Import Project="..\Win32AppHelpers\inc\include_this_dir.targets" />
```

#### Using the NuGet feed

Setup a package feed to reference `https://chrisguzak.pkgs.visualstudio.com/_packaging/ChrisGuzak/nuget/v3/index.json` 
and then add `Win32AppHelpers` using the NuGet package manager.

## Documentation

### win32app/win32_app_helpers.h Functions

These functions use a template parameter to define the class that represents the window being created. 
That class must have a member variable of type `wil::unique_hwnd` named `m_window` as these functions 
refer to that and manage its lifetime.

#### create_top_level_window()

Create the window and store it in `m_window`.

#### create_top_level_window_for_xaml()

Create the window and store it in `m_window`, but use window styles that optimize using Xaml for the whole client area.

#### enter_simple_message_loop()

Message loop every Win32 GUI thread must implement.

### win32app/reference_waiter.h

`reference_waiter` is useful for multi-window applications that create a thread for each top level window.
This enables coordinating process rundown, waiting for the last window to be closed.

```cpp
struct AppWindow
{
...
    inline static reference_waiter m_appThreadsWaiter;
    inline static std::mutex m_lock;
    inline static std::vector<std::thread> m_threads;

    template <typename Lambda>
    static void StartThread(Lambda&& fn)
    {
        std::unique_lock<std::mutex> holdLock(m_lock);
        m_threads.emplace_back([fn = std::forward<Lambda>(fn), threadRef = m_appThreadsWaiter.take_reference()]() mutable
        {
            std::forward<Lambda>(fn)();
        });
    }

    static void WaitUntilAllWindowsAreClosed()
    {
        m_appThreadsWaiter.wait_until_zero();

        // now we are safe to iterate over m_threads as it can't change any more
        for (auto& thread : AppWindow::m_threads)
        {
            thread.join();
        }
    }

_Use_decl_annotations_ int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR lpCmdLine, int nCmdShow)
{
    auto coInit = wil::CoInitializeEx();

    AppWindow::StartThread([nCmdShow]()
    {
        auto coInit = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
        std::make_unique<AppWindow>()->Show(nCmdShow);
    });

    AppWindow::WaitUntilAllWindowsAreClosed();
    return 0;
}
```

### win32app/ResizeableDialog.h

Helpers for making dialog template based applications that support resizing.

### win32app/Win32UI.h

An add-hoc set of helpers for dealing with Win32. For example a function to return the window title as a `std::wstring`.

### win32app/LogWindow.h

A way to use a listview in group mode to log output.

