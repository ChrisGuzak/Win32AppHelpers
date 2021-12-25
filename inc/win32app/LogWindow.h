#pragma once
// 1) define the GroupId enum
//      enum class GroupId = { Default, Id1, Id12, ... };
// 2) define the group mapping structure
//      struct GroupInfo
//      {
//          GroupId Id;
//          PCWSTR Name;
//      };
//
// 3) declare c_groupInfos[]
//    const GroupInfo c_groupInfos[] = { };
// 4) declare the member var that represents the log window
//      LogWindow<GroupInfo, GroupId> _logWindow
// 5) in the constructor of your main window call LogWindow's constructor
//      _logWindow(c_groupInfos, ARRAYSIZE(c_groupInfos))
// 6) in WM_INITDIALOG call _logWindow.InitListView(GetDlgItem(_hdlg, IDC_LISTVIEW))
// 7) in your .RC file add the definition of the listview window control
//    CONTROL "",IDC_LISTVIEW,"SysListView32",WS_CLIPCHILDREN | WS_TABSTOP, 7,7,281,191
// 8) to enable the right click menu in the log window forward the WM_NOTIFY message
//    to LogWindow list this
//        case WM_NOTIFY:
//            {
//                auto notifyMessage = reinterpret_cast<NMHDR*>(lParam);
//                if (notifyMessage->idFrom == IDC_LISTVIEW)
//                {
//                    _logWindow.OnNotify(notifyMessage);
//                }
//            }
//            break;
#include <commctrl.h>
#include <strsafe.h>
#include <wil/stl.h>
#include <wil/resource.h>

template <class TGroupIDMap, class TGroupID> class LogWindow
{
public:
    LogWindow(TGroupIDMap const* groupInfo, size_t count) :
        m_groupInfo(groupInfo), m_groupInfoCount(count)
    {
    }

    void InitListView(HWND hwndList)
    {
        m_hwndList = hwndList;
        // Enable ListView for Grouping mode.
        SetWindowLongPtrW(m_hwndList, GWL_STYLE, GetWindowLongPtr(m_hwndList, GWL_STYLE) |
                        LVS_REPORT | LVS_NOSORTHEADER | LVS_SHOWSELALWAYS);
        ListView_SetExtendedListViewStyle(m_hwndList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        ListView_EnableGroupView(m_hwndList, TRUE);

        // Setup up common values.
        LVCOLUMN lvc = {};
        lvc.mask     = LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT; // Links are not active in this dialog, so just show text without markup
        lvc.fmt      = LVCFMT_LEFT;
        lvc.pszText  = const_cast<PWSTR>(L"Name");

        // Add Column 0
        lvc.iSubItem = 0;
        ListView_InsertColumn(m_hwndList, 0, &lvc);

        // Add Column 1
        lvc.iSubItem = 1;
        lvc.pszText = const_cast<PWSTR>(L"Value");
        ListView_InsertColumn(m_hwndList, 1, &lvc);

        AutoAdjustListView();

        // Init group IDs and display names
        for (auto const& group : wil::make_range(m_groupInfo, m_groupInfoCount))
        {
            LogGroup(group.Id, group.Name);
        }
    }

    void AutoAdjustListView()
    {
        // Auto-adjust the column widths making sure that the first column doesn't
        // make itself too big.

        ListView_SetColumnWidth(m_hwndList, 0, LVSCW_AUTOSIZE_USEHEADER);

        RECT rect;
        BOOL bRet = GetClientRect(m_hwndList, &rect);
        if (bRet)
        {
            LVCOLUMN lvc;
            lvc.mask = LVCF_WIDTH;
            bRet = ListView_GetColumn(m_hwndList, 0, &lvc);
            if (bRet)
            {
                int iSize = rect.right / 2;
                int cxScroll = GetSystemMetrics(SM_CXVSCROLL);

                if (lvc.cx > iSize)
                {
                    ListView_SetColumnWidth(m_hwndList, 0, iSize);
                    ListView_SetColumnWidth(m_hwndList, 1, iSize - cxScroll);
                }
                else
                {
                    ListView_SetColumnWidth(m_hwndList, 1, rect.right - lvc.cx - cxScroll);
                }
            }
        }

        if (!bRet)
        {
            ListView_SetColumnWidth(m_hwndList, 1, LVSCW_AUTOSIZE_USEHEADER);
        }
    }

    void LogGroup(TGroupID groupId, PCWSTR pszGroupName)
    {
        LVGROUP lvg = { sizeof(lvg) };
        lvg.mask      = LVGF_HEADER | LVGF_GROUPID | LVGF_STATE;
        lvg.state     = LVGS_COLLAPSIBLE;

        int const iGroupID = (groupId == TGroupID::Default) ? static_cast<int>(ListView_GetGroupCount(m_hwndList)) : static_cast<int>(groupId);
        lvg.iGroupId  = iGroupID;
        lvg.pszHeader = const_cast<PWSTR>(pszGroupName);
        ListView_InsertGroup(m_hwndList, -1, &lvg);
    }

    void LogMessage(TGroupID groupId, PCWSTR name, PCWSTR value)
    {
        // Add an item name
        LVITEM lvi = {};
        lvi.mask      = LVIF_TEXT | LVIF_GROUPID;
        lvi.iItem     = MAXLONG;

        int const iGroupID = (groupId == TGroupID::Default) ?
            static_cast<int>(ListView_GetGroupCount(m_hwndList)) - 1 :  // groups are numbered 0, 1, ... n-1
            static_cast<int>(groupId);

        lvi.iGroupId  = iGroupID;
        lvi.pszText   = const_cast<PWSTR>(name);

        int iItem = ListView_InsertItem(m_hwndList, &lvi);
        if (-1 != iItem)
        {
            // Add the formatted value.
            ListView_SetItemText(m_hwndList, iItem, 1, const_cast<PWSTR>(value));

            if (m_debugOutput)
            {
                OutputDebugStringW(name);
                OutputDebugStringW(L"\t");
                OutputDebugStringW(value);
                OutputDebugStringW(L"\r\n");
            }
        }
    }

    template<typename... args_t>
    void LogMessagePrintf(TGroupID groupId, PCWSTR name, _Printf_format_string_ PCWSTR format, args_t&&... args)
    {
        auto str = wil::str_printf<std::wstring>(format, std::forward<args_t>(args)...);
        LogMessage(groupId, name, str.c_str());
    }

    void ResetContents()
    {
        ListView_DeleteAllItems(m_hwndList);
    }

    void CollapseAllGroups()
    {
        int const countGroups = static_cast<int>(ListView_GetGroupCount(m_hwndList));
        for (int i = 0; i < countGroups; i++)
        {
            ListView_SetGroupState(m_hwndList, i, LVGS_COLLAPSED, LVGS_COLLAPSED);
        }
    }

    void SetDebugOutput(bool fDebugOutput)
    {
        m_debugOutput = fDebugOutput;
    }

    PWSTR GetText(bool fSelectionOnly)
    {
        size_t const charCountAllocated = 64 * 1024;   // fixed size buffer for simplicity of impl
        PWSTR clipboardText = (PWSTR)GlobalAlloc(GPTR, charCountAllocated * sizeof(*clipboardText));
        if (clipboardText)
        {
            PWSTR output = clipboardText;  // accumulate results using this pointer
            size_t chareCountRemaining = charCountAllocated;  // size left in buffer
            int iLastGroupId = -1;

            HRESULT hr = S_OK;
            const int itemCount = ListView_GetItemCount(m_hwndList);
            for (int i = 0; (i < itemCount) && SUCCEEDED(hr); i++)
            {
                if (fSelectionOnly ? (LVIS_SELECTED == ListView_GetItemState(m_hwndList, i, LVIS_SELECTED)) : true)
                {
                    LVCOLUMN column = { LVCF_WIDTH }; // query a dummy value so we can probe for columns presence
                    for (int j = 0; ListView_GetColumn(m_hwndList, j, &column); j++)
                    {
                        wchar_t szBuffer[4096];
                        LV_ITEM item;
                        item.iItem = i;
                        item.iSubItem = j;
                        item.mask = LVIF_TEXT | LVIF_GROUPID;
                        item.pszText = szBuffer;
                        item.cchTextMax = ARRAYSIZE(szBuffer);
                        if (ListView_GetItem(m_hwndList, &item))
                        {
                            if ((j == 0) && (item.iGroupId != iLastGroupId))
                            {
                                if (iLastGroupId != -1)
                                {
                                    hr = StringCchCatExW(output, chareCountRemaining, L"\r\n", &output, &chareCountRemaining, 0); // line break after each group
                                }
                                iLastGroupId = item.iGroupId;

                                if (SUCCEEDED(hr))
                                {
                                    wchar_t groupName[64];
                                    LVGROUP groupInfo = { sizeof(groupInfo) };
                                    groupInfo.mask = LVGF_HEADER;
                                    groupInfo.pszHeader = groupName;
                                    groupInfo.cchHeader = ARRAYSIZE(groupName);
                                    if (ListView_GetGroupInfo(m_hwndList, item.iGroupId, &groupInfo))
                                    {
                                        hr = StringCchCatExW(output, chareCountRemaining, groupName, &output, &chareCountRemaining, 0);
                                        if (SUCCEEDED(hr))
                                        {
                                            hr = StringCchCatExW(output, chareCountRemaining, L"\r\n", &output, &chareCountRemaining, 0);
                                        }
                                    }
                                }
                            }

                            if (SUCCEEDED(hr))
                            {
                                hr = StringCchCatExW(output, chareCountRemaining, L"\t", &output, &chareCountRemaining, 0);
                                if (SUCCEEDED(hr))
                                {
                                    hr = StringCchCatExW(output, chareCountRemaining, szBuffer, &output, &chareCountRemaining, 0);
                                }
                            }
                        }
                    }
                    if (SUCCEEDED(hr))
                    {
                        hr = StringCchCatExW(output, chareCountRemaining, L"\r\n", &output, &chareCountRemaining, 0);
                    }
                }
            }
        }
        return clipboardText;
    }

    void CopyTextToClipboard(bool fSelectionOnly, HWND hwnd)
    {
        wil::unique_hglobal_string content{ GetText(fSelectionOnly) };
        if (OpenClipboard(hwnd))
        {
            EmptyClipboard();
            if (SetClipboardData(CF_UNICODETEXT, content.get()) == content.get())
            {
                content.release(); // ownership passed to clipboard
            }
            CloseClipboard();
        }
    }

    void OnNotify(NMHDR const *pnm)
    {
        if (pnm->code == NM_RCLICK)
        {
            auto pnmItem = reinterpret_cast<NMITEMACTIVATE const *>(pnm);
            POINT ptMenu = pnmItem->ptAction;
            ClientToScreen(pnmItem->hdr.hwndFrom, &ptMenu);

            wil::unique_hmenu menu{ CreatePopupMenu() };
            AppendMenuW(menu.get(), MF_ENABLED, 1, L"Copy");
            AppendMenuW(menu.get(), MF_ENABLED, 2, L"Copy All");

            int idCmd = TrackPopupMenu(menu.get(), TPM_RETURNCMD, ptMenu.x, ptMenu.y, 0, m_hwndList, NULL);
            if (idCmd == 1)
            {
                CopyTextToClipboard(true, GetParent(m_hwndList));
            }
            else if (idCmd == 2)
            {
                CopyTextToClipboard(false, GetParent(m_hwndList));
            }
        }
    }

    void SetRedraw(BOOL fRedraw)
    {
        SendMessageW(m_hwndList, WM_SETREDRAW, fRedraw, 0);
    }

private:
    HWND m_hwndList = nullptr;
    TGroupIDMap const* m_groupInfo;
    size_t m_groupInfoCount{};
    bool m_debugOutput = false;
};
