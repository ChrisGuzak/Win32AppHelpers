#pragma once
#include <winuser.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.h>

inline const wchar_t* MakeIntResourceId(int i)
{
    return reinterpret_cast<const wchar_t*>(static_cast<ULONG_PTR>(i));
}

#define XAMLRESOURCE  256 // see resource.h, try to get rid of this by using a standard "file" type.

template<typename T = winrt::Windows::UI::Xaml::UIElement>
T LoadXamlResource(HMODULE mod, uint32_t id, const wchar_t* type = MakeIntResourceId(XAMLRESOURCE))
{
    auto rc = ::FindResourceW(mod, MakeIntResourceId(id), type);
    winrt::check_bool(rc != nullptr);
    HGLOBAL rcData = ::LoadResource(mod, rc);
    winrt::check_bool(rcData != nullptr);
    auto textStart = static_cast<wchar_t*>(::LockResource(rcData));
    auto size = SizeofResource(mod, rc);
    winrt::hstring text{ textStart, size / sizeof(*textStart) }; // need a copy to null terminate, see if this can be avoided
    return winrt::Windows::UI::Xaml::Markup::XamlReader::Load(text).as<T>();
}
