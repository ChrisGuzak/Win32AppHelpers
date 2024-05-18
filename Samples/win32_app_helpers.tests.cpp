#include "pch.h"
#include <win32app/win32_app_helpers.h>

// Compile only tests. Since the design is template based a lot of
// validation can be done by making sure things compile (and things don't).

// use this to iterate on adding new messages, this demonstrates what was used
// to develop WM_SIZE.

#include <win32app/XamlHostWindow.h>

namespace win32app::details
{

inline void TestUsageUnrolled()
{
    struct Window
    {
        LRESULT Size(unsigned short, unsigned short)
        {
            static_assert(msg<WM_SIZE, Window>::is_valid);
            return 0;
        }
    };

    using size = msg<WM_SIZE, Window>;
    static_assert(size::is_valid);
}

template <unsigned int value>
void TestOneMessage()
{
    struct Window
    {
        LRESULT Size(unsigned short, unsigned short)
        {
            static_assert(msg<WM_SIZE, decltype(*this)>::is_valid);
            return 0;
        }
        LRESULT Move(unsigned short, unsigned short)
        {
            static_assert(msg<WM_MOVE, decltype(*this)>::is_valid);
            return 0;
        }
        LRESULT Create()
        {
            static_assert(msg<WM_CREATE, decltype(*this)>::is_valid);
            return 0;
        }
        LRESULT Destroy()
        {
            static_assert(msg<WM_DESTROY, decltype(*this)>::is_valid);
            return 0;
        }
        LRESULT Paint(HDC hdc, const PAINTSTRUCT& ps)
        {
            static_assert(msg<WM_PAINT, decltype(*this)>::is_valid);
            return 0;
        }
        LRESULT Command(unsigned int id)
        {
            static_assert(msg<WM_COMMAND, decltype(*this)>::is_valid);
            return 0;
        }
        LRESULT DeviceChange(unsigned int dbt /* DBT_*/, void* input)
        {
            static_assert(msg<WM_DEVICECHANGE, decltype(*this)>::is_valid);
            return 0;
        }
    };

    static_assert(msg<value, Window>::is_valid, "'value' is not supported, update code above to add it.");
}

inline void TestUsage()
{
    {
        struct Window
        {
        };
        // negative test for unsupported messages
        using neverDefined = msg<WM_SPOOLERSTATUS, Window>;
        static_assert(!neverDefined::is_valid);
        static_assert(neverDefined::value == WM_SPOOLERSTATUS);
    }

    // Enable and verify that the static assert message is useful
    // TestOneMessage<WM_SPOOLERSTATUS>();

    TestOneMessage<WM_SIZE>();
    TestOneMessage<WM_MOVE>();
    TestOneMessage<WM_DESTROY>();
    TestOneMessage<WM_PAINT>();
    TestOneMessage<WM_COMMAND>();
    TestOneMessage<WM_DEVICECHANGE>();
}
} // namespace win32app::details

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

void TestSimpleCase(int nCmdShow = SW_SHOWDEFAULT)
{
    std::make_unique<SimplestAppWindow>()->Show(nCmdShow);
}
