#pragma once

#include <wil/cppwinrt.h>
#include <wil/com.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef GetCurrentTime

#include <wil/win32_helpers.h>
#include <wil/cppwinrt_helpers.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.System.h>

#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h> // COM interop

#include <mutex>

#include <wil/resource.h>

#include "win32_app_helpers.h"
#include "reference_waiter.h"
#include "utf8_helpers.h"

struct XamlHostWindow : public std::enable_shared_from_this<XamlHostWindow>
{
    inline static INIT_ONCE m_initOnce{};
    inline static winrt::Windows::UI::Xaml::Application s_app{nullptr};

    static void EnsureAppInitialized()
    {
        // The Application must be created early in the lifetime so that
        // Xaml does not create it on demand later.
        wil::init_once(m_initOnce, []() {
            s_app = winrt::Windows::UI::Xaml::Application();
        });
    }

    template<typename TLambda>
    static void RunOnUIThread(TLambda fn, bool closeWindowWhenDone = true)
    {
        wil::unique_event windowRunning{wil::EventOptions::None};

        std::shared_ptr<XamlHostWindow> appWindow;

        auto nCmdShow = SW_SHOWNORMAL;

        XamlHostWindow::StartThreadAsync([&](auto&& queueController) {
            appWindow = std::make_shared<XamlHostWindow>(std::move(queueController));
            appWindow->Show(nCmdShow);
            windowRunning.SetEvent();
        });

        windowRunning.wait();

        winrt::Windows::Foundation::IAsyncAction fnAction;

        appWindow->RunAsync([&](auto& window) noexcept {
                if constexpr (std::is_same_v<std::invoke_result_t<decltype(fn), XamlHostWindow>, decltype(fnAction)>)
                {
                    fnAction = fn(window);
                }
                else
                {
                    fn(window);
                }
            }).get();

        if (fnAction)
        {
            fnAction.get();
        }

        if (closeWindowWhenDone)
        {
            appWindow->RunAsync([](auto& window) noexcept { window.m_window.reset(); }).get();
        }

        XamlHostWindow::wait_until_zero();
    }

    XamlHostWindow(winrt::Windows::System::DispatcherQueueController queueController) : m_queueController(std::move(queueController))
    {
        EnsureAppInitialized();
        m_xamlManager = winrt::Windows::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
    }

    LRESULT Create()
    {
        using namespace winrt::Windows::UI::Xaml;
        using namespace winrt::Windows::UI::Xaml::Controls;
        using namespace winrt::Windows::UI::Xaml::Input;

        // WindowsXamlManager must be used if multiple islands are created on the thread or in the process.
        // It must be constructed before the first DesktopWindowXamlSource.
        m_xamlSource = winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource();

        auto interop = m_xamlSource.as<IDesktopWindowXamlSourceNative>();
        THROW_IF_FAILED(interop->AttachToWindow(m_window.get()));
        THROW_IF_FAILED(interop->get_WindowHandle(&m_xamlSourceWindow));

        auto contentText = skip_utf8_bom(get_resource_view(L"AppWindow.xaml", RT_RCDATA));
        auto contentTextWide = from_utf8(contentText);
        auto content = winrt::Windows::UI::Xaml::Markup::XamlReader::Load(contentTextWide).as<winrt::Windows::UI::Xaml::UIElement>();
        m_xamlSource.Content(content);

        m_status = content.as<FrameworkElement>().FindName(L"Status").as<TextBlock>();

        return 0;
    }

    LRESULT Size(WORD dx, WORD dy)
    {
        SetWindowPos(m_xamlSourceWindow, nullptr, 0, 0, dx, dy, SWP_SHOWWINDOW);
        return 0;
    }

    void Show(int nCmdShow)
    {
        win32app::create_top_level_window_for_xaml(*this, L"Win32XamlAppWindow", L"Win32 Xaml App");
        const auto dpi = GetDpiForWindow(m_window.get());
        const int dx = (600 * dpi) / 96;
        const int dy = (800 * dpi) / 96;
        SetWindowPos(m_window.get(), nullptr, 0, 0, dx, dy, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW);

        AddWeakRef(this);
        m_selfRef = shared_from_this();
        m_appRefHolder.emplace(m_appThreadsWaiter.take_reference());
    }

    LRESULT Destroy()
    {
        RemoveWeakRef(this);

        // Close the DesktopWindowXamlSource and the WindowsXamlManager.  This will start Xaml's run down since all the
        // DWXS/WXM on the thread will now be closed.  Xaml's run-down is async, so we need to keep the message loop running.
        m_xamlSource.Close();
        m_xamlManager.Close();

        [](auto that) -> winrt::fire_and_forget {
            auto delayedRelease = std::move(that->m_selfRef);
            co_await that->m_queueController.ShutdownQueueAsync();
        }(this);

        m_appRefHolder.reset();

        return 0;
    }

    winrt::Windows::System::DispatcherQueue DispatcherQueue() const
    {
        return m_queueController.DispatcherQueue();
    }

    auto Content() const
    {
        return m_xamlSource.Content();
    }

    void ReplaceContent(winrt::Windows::UI::Xaml::UIElement const& newContent)
    {
        m_xamlSource.Content(newContent);
    }

    template <typename Lambda>
    static winrt::fire_and_forget StartThreadAsync(Lambda fn)
    {
        auto queueController = winrt::Windows::System::DispatcherQueueController::CreateOnDedicatedThread();
        co_await wil::resume_foreground(queueController.DispatcherQueue());
        fn(std::move(queueController));
    }

    static std::vector<std::shared_ptr<XamlHostWindow>> GetAppWindows()
    {
        std::vector<std::shared_ptr<XamlHostWindow>> result;

        auto lock = std::lock_guard<std::mutex>(m_appWindowLock);
        result.reserve(m_appWindows.size());
        for (auto& weakWindow : m_appWindows)
        {
            if (auto strong = weakWindow.lock())
            {
                result.emplace_back(std::move(strong));
            }
        }
        return result;
    }

    template <typename Lambda>
    winrt::Windows::Foundation::IAsyncAction RunAsync(Lambda fn)
    {
        co_await wil::resume_foreground(DispatcherQueue());
        fn(*this);
    }

    template <typename Lambda>
    static winrt::Windows::Foundation::IAsyncAction BroadcastAsync(Lambda fn)
    {
        auto windows = GetAppWindows();
        for (const auto& windowRef : windows)
        {
            co_await wil::resume_foreground(windowRef.get()->DispatcherQueue());
            fn(*windowRef.get());
        }
    }

    static auto take_reference()
    {
        return m_appThreadsWaiter.take_reference();
    }

    static void wait_until_zero()
    {
        m_appThreadsWaiter.wait_until_zero();
    }

    static void AddWeakRef(XamlHostWindow* that)
    {
        auto lock = std::lock_guard<std::mutex>(m_appWindowLock);
        XamlHostWindow::m_appWindows.emplace_back(that->weak_from_this());
    }

    static void RemoveWeakRef(XamlHostWindow* that)
    {
        auto lock = std::lock_guard<std::mutex>(m_appWindowLock);
        m_appWindows.erase(std::find_if(m_appWindows.begin(), m_appWindows.end(), [&](auto&& weakOther) {
            if (auto strong = weakOther.lock())
            {
                return strong.get() == that;
            }
            return false;
        }));
    }

    wil::unique_hwnd m_window;

private:
    HWND m_xamlSourceWindow{}; // This is owned by m_xamlSource, destroyed when Close() is called.

    std::shared_ptr<XamlHostWindow> m_selfRef;                                    // needed to extend lifetime during async rundown
    std::optional<reference_waiter::reference_waiter_holder> m_appRefHolder; // need to ensure lifetime of the app process

    // This is needed to coordinate the use of Xaml from multiple threads.
    winrt::Windows::UI::Xaml::Hosting::WindowsXamlManager m_xamlManager{nullptr};
    winrt::Windows::System::DispatcherQueueController m_queueController{nullptr};
    winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource m_xamlSource{nullptr};
    winrt::Windows::UI::Xaml::Controls::TextBlock m_status{nullptr};

    inline static reference_waiter m_appThreadsWaiter;
    inline static std::mutex m_appWindowLock;
    inline static std::vector<std::weak_ptr<XamlHostWindow>> m_appWindows;
};
