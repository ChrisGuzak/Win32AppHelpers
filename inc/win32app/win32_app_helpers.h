#pragma once
#include <winuser.h>
#undef GetCurrentTime // avoid conflict with winuser.h

#include <wil/win32_helpers.h>
#include <string_view>
#include <winrt/Windows.UI.Xaml.Hosting.h>

#include "is_detected.h"

namespace win32app
{
namespace details
{
    // Window Message dispatch implementation
    // These types map message values to a function prototype that can handle them.
    // Clients implement the function of the given name and parameter list and
    // messages will get dispatched to them.
    //
    // The 'wparam' and 'lparam' are converted into message specific values and
    // passed to the function.
    // Conversions are to the Win32 primitive representation to enable the most flexibility.

        template<unsigned int value_, typename T> struct msg
    {
        static constexpr unsigned int value = value_;
        static constexpr bool is_valid = false;
    };

        template<typename T> struct msg<WM_SIZE, T>
    {
            template<typename T> using resultT = decltype(std::declval<T>().Size(std::declval<unsigned short>(), std::declval<unsigned short>()));
        static constexpr bool is_valid = is_detected<resultT, T>::value;
    };

        template<typename T> struct msg<WM_MOVE, T>
    {
            template<typename T> using resultT = decltype(std::declval<T>().Move(std::declval<unsigned short>(), std::declval<unsigned short>()));
        static constexpr bool is_valid = is_detected<resultT, T>::value;
    };

        template<typename T> struct msg<WM_CREATE, T>
    {
            template<typename T> using resultT = decltype(std::declval<T>().Create());
        static constexpr bool is_valid = is_detected<resultT, T>::value;
    };

        template<typename T> struct msg<WM_DESTROY, T>
    {
            template<typename T> using resultT = decltype(std::declval<T>().Destroy());
        static constexpr bool is_valid = is_detected<resultT, T>::value;
    };

        template<typename T> struct msg<WM_PAINT, T>
    {
            template<typename T> using resultT = decltype(std::declval<T>().Paint(std::declval<HDC>(), std::declval<const PAINTSTRUCT&>()));
        static constexpr bool is_valid = is_detected<resultT, T>::value;
    };

        template<typename T> struct msg<WM_COMMAND, T>
        {
            template<typename T> using resultT = decltype(std::declval<T>().Command(std::declval<unsigned short>()));
            static constexpr bool is_valid = is_detected<resultT, T>::value;
        };

        template<typename T> struct msg<WM_DEVICECHANGE, T>
        {
            template<typename T> using resultT = decltype(std::declval<T>().DeviceChange(std::declval<unsigned int>(), std::declval<void*>()));
            static constexpr bool is_valid = is_detected<resultT, T>::value;
        };

        template<typename T> struct msg<-1, T>
        {
            // UINT32, WPARAM, LPARAM
            template<typename T> using resultT = decltype(std::declval<T>().HandleMessage(std::declval<UINT32>(), std::declval<WPARAM>(), std::declval<LPARAM>()));
            static constexpr bool is_valid = is_detected<resultT, T>::value;
        };

        template<typename T>
        auto HandleMessage(T* that, UINT32 message, WPARAM wparam, LPARAM lparam)
        {
            switch (message)
            {
                case WM_MOVE: if constexpr (msg<WM_MOVE, T>::is_valid)
                {
                    const WORD dx = LOWORD(lparam), dy = HIWORD(lparam);
                    return that->Move(dx, dy);
                }
                break;

                case WM_SIZE: if constexpr (msg<WM_SIZE, T>::is_valid)
                {
                    const WORD dx = LOWORD(lparam), dy = HIWORD(lparam);
                    return that->Size(dx, dy);
                }
                break;

                case WM_CREATE: if constexpr (msg<WM_CREATE, T>::is_valid)
                {
                    return that->Create();
                }
                break;

                case WM_DESTROY: if constexpr (msg<WM_DESTROY, T>::is_valid)
                {
                    return that->Destroy();
                }
                break;

                case WM_PAINT: if constexpr (msg<WM_PAINT, T>::is_valid)
                {
                    PAINTSTRUCT ps{};
                    auto hdc{ BeginPaint(that->m_window.get(), &ps) };
                    auto result = that->Paint(hdc, ps);
                    EndPaint(that->m_window.get(), &ps);
                    return result;
                }
                break;

                case WM_COMMAND: if constexpr (msg<WM_COMMAND, T>::is_valid)
                {
                    return that->Command(LOWORD(wparam));
                }
                break;

                case WM_DEVICECHANGE: if constexpr (msg<WM_DEVICECHANGE, T>::is_valid)
                {
                    return that->DeviceChange(static_cast<UINT>(wparam), reinterpret_cast<void*>(lparam));
                }
                break;

            default:
            {
                if constexpr (msg<-1, T>::is_valid)
                {
                    return that->HandleMessage(message, wparam, lparam);
                }
            }
            break;
            }
            return DefWindowProcW(that->m_window.get(), message, wparam, lparam);
        }

        template <typename T>
        void create_top_level_window(T& instance, DWORD styles, DWORD exStyles, PCWSTR className, PCWSTR title = nullptr)
        {
            WNDCLASSEXW wcex{ sizeof(wcex) };
            wcex.style = CS_HREDRAW | CS_VREDRAW;
            wcex.lpfnWndProc = [](HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept -> LRESULT
            {
                if (message == WM_NCCREATE)
                {
                    auto cs = reinterpret_cast<CREATESTRUCT*>(lparam);
                    auto that = static_cast<T*>(cs->lpCreateParams);
                    that->m_window.reset(window); // take ownership
                    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(that));
                }
                else if (message == WM_NCDESTROY)
                {
                    SetWindowLongPtrW(window, GWLP_USERDATA, 0);
                }
                else if (auto that = reinterpret_cast<T*>(GetWindowLongPtrW(window, GWLP_USERDATA)))
                {
                    return HandleMessage(that, message, wparam, lparam);
                }

                return DefWindowProcW(window, message, wparam, lparam);
            };
            wcex.hInstance = wil::GetModuleInstanceHandle();
            auto imageResMod = LoadLibraryExW(L"imageres.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_AS_DATAFILE);
            wcex.hIcon = LoadIconW(imageResMod, reinterpret_cast<const wchar_t*>(5206)); // App Icon
            wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
            wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wcex.lpszClassName = className;

            RegisterClassExW(&wcex);

            THROW_LAST_ERROR_IF(!CreateWindowExW(exStyles, className, L"Win32 App", styles,
                CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, wil::GetModuleInstanceHandle(),
                &instance));
        }
    }

// The created window is stored in T.m_window (must be wil::unique_hwnd).
template <typename T>
void create_top_level_window(T& instance, PCWSTR className, PCWSTR title = nullptr)
{
    return details::create_top_level_window<T>(instance, WS_OVERLAPPEDWINDOW, 0, className, title);
}

// the created window is stored in T.m_window
// T must have wil::unique_hwnd m_window.
template <typename T>
void create_top_level_window_for_xaml(T& instance, PCWSTR className, PCWSTR title = nullptr)
{
    return details::create_top_level_window<T>(instance,
        WS_OVERLAPPEDWINDOW, WS_EX_NOREDIRECTIONBITMAP, className, title);
}

// T must have wil::unique_hwnd m_window.
template <typename T>
void enter_simple_message_loop(T& instance, UINT nCmdShow)
{
    auto w = instance.m_window.get();
    ShowWindow(w, nCmdShow);
    UpdateWindow(w);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

// T must have wil::unique_hwnd m_window.
template <typename T>
void enter_com_message_loop(T& instance, UINT nCmdShow, wil::unique_event& shutdownSignal)
{
    auto w = instance.m_window.get();
    ShowWindow(w, nCmdShow);
    UpdateWindow(w);

    // Ensure a continuous Xaml lifetime on this thread.
    auto xamlManager = winrt::Windows::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();

    DWORD index{};
    HANDLE waitArray[]{ shutdownSignal.get() };
    FAIL_FAST_IF_FAILED(CoWaitForMultipleHandles(COWAIT_DISPATCH_CALLS | COWAIT_DISPATCH_WINDOW_MESSAGES, INFINITE,
        ARRAYSIZE(waitArray), waitArray, &index));

    // Need an extra message loop to enable Xaml to finish its rundown.
    PostQuitMessage(0); // ensures we terminate the loop when Xaml is done.
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        DispatchMessageW(&msg);
    }
}

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

}

