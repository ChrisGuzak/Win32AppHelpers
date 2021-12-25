#pragma once
#include <mutex>
#include <condition_variable>
#include <algorithm>

/*
reference_waiter

Useful for multi-window applications that create a thread for each top level window.
This enables coordinating process rundown, waiting for the last window to be closed.

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

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR lpCmdLine, int nCmdShow)
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
*/

struct reference_waiter
{
    void wait_until_zero()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [&] { return m_count == 0; });
    }

    struct reference_waiter_holder
    {
        reference_waiter_holder(reference_waiter& s) : m_s(s)
        {
            add();
        }

        ~reference_waiter_holder()
        {
            subtract();
        }

        reference_waiter_holder() = delete;
        reference_waiter_holder(const reference_waiter_holder&) = delete;
        const reference_waiter_holder& operator=(const reference_waiter_holder&) = delete;

        reference_waiter_holder(reference_waiter_holder&& other) noexcept : m_s(other.m_s)
        {
            add();
        }
        reference_waiter_holder& operator=(reference_waiter_holder&&) noexcept
        {
            add();
            return *this;
        }

    private:
        void add()
        {
            std::unique_lock<std::mutex> lock(m_s.m_mutex);
            m_s.m_count++;
            m_s.m_cv.notify_all(); // notify the waiting thread
        }

        void subtract()
        {
            std::unique_lock<std::mutex> lock(m_s.m_mutex);
            m_s.m_count--;
            m_s.m_cv.notify_all(); // notify the waiting thread
        }

        reference_waiter& m_s;
    };

    [[nodiscard]] reference_waiter_holder take_reference()
    {
        return reference_waiter_holder(*this);
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    int m_count = 0;
};
