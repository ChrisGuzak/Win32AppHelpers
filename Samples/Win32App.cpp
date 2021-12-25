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

_Use_decl_annotations_
int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR lpCmdLine, int nCmdShow)
{
    auto coInit = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
    std::make_unique<AppWindow>()->Show(nCmdShow);
    return 0;
}
