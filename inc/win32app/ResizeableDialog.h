// to enable a dialog to be re-sizable do the following after including this header:
//
// 1) in your class declare
//      private:
//          static const ANCHOR c_anchors[5];
//          RECT m_anchorOffsets[ARRAYSIZE(c_anchors)];
//
// 2) declare an entry in this table for every control in your dialog, for example
//
//      const ANCHOR CShellQueryInspectorApp::c_anchors[] =
//      {
//          { IDC_QUERY,        AF_LEFT | AF_BOTTOM },
//          { IDC_OPEN_ITEM,    AF_LEFT | AF_BOTTOM },
//          { IDC_PICK,         AF_LEFT | AF_BOTTOM },
//          { IDC_STATIC,       AF_LEFT | AF_RIGHT | AF_TOP | AF_BOTTOM },
//          { IDC_LISTVIEW,     AF_LEFT | AF_RIGHT | AF_TOP | AF_BOTTOM },
//      };
//
// 3) in your DialogProc add
//      case WM_SIZE:
//          OnSize(_hdlg, c_anchors, ARRAYSIZE(c_anchors), m_anchorOffsets);
//          break;
//
//      case WM_INITDIALOG:
//          InitResizeData(_hdlg, c_anchors, ARRAYSIZE(c_anchors), m_anchorOffsets);
//          break;
//

// Windows are resized relative to the edges identified by these flags
enum ANCHOR_FLAGS
{
    AF_NONE             = 0x00,
    AF_LEFT             = 0x01,
    AF_RIGHT            = 0x02,
    AF_LEFT_AND_RIGHT   = 0x03,
    AF_TOP              = 0x04,
    AF_BOTTOM           = 0x08,
    AF_TOP_AND_BOTTOM   = 0x0C,
};

DEFINE_ENUM_FLAG_OPERATORS(ANCHOR_FLAGS);

struct ANCHOR
{
    DWORD idControl;
    ANCHOR_FLAGS aff;
};

inline void GetWindowRectInClient(HWND hwnd, _Out_ RECT* prc)
{
    GetWindowRect(hwnd, prc);
    MapWindowPoints(GetDesktopWindow(), GetParent(hwnd), (POINT*)prc, 2);
}

// retrieve the HINSTANCE for the current DLL or EXE using this symbol that
// the linker provides for every module, avoids the need for a global HINSTANCE variable
// and provides access to this value for static libraries
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
inline HINSTANCE GetModuleHINSTANCE() { return reinterpret_cast<HINSTANCE>(&__ImageBase); }

inline void InitResizeData(HWND hdlg, const ANCHOR rgAnchors[], size_t cAnchors, _Out_ RECT rgAnchorOffsets[])
{
    // record anchor information
    RECT rcClient = {};
    GetClientRect(hdlg, &rcClient);

    for (DWORD iAnchor = 0; iAnchor < cAnchors; iAnchor++)
    {
        GetWindowRectInClient(GetDlgItem(hdlg, rgAnchors[iAnchor].idControl), &rgAnchorOffsets[iAnchor]);

        switch (rgAnchors[iAnchor].aff & AF_LEFT_AND_RIGHT)
        {
        case AF_LEFT_AND_RIGHT:
            rgAnchorOffsets[iAnchor].left -= rcClient.left;    // offset
            rgAnchorOffsets[iAnchor].right -= rcClient.right;  // offset
            break;

        case AF_RIGHT:
            rgAnchorOffsets[iAnchor].left = rgAnchorOffsets[iAnchor].right - rgAnchorOffsets[iAnchor].left;  // width
            rgAnchorOffsets[iAnchor].right -= rcClient.right;  // offset
            break;
        }

        switch (rgAnchors[iAnchor].aff & AF_TOP_AND_BOTTOM)
        {
        case AF_TOP_AND_BOTTOM:
            rgAnchorOffsets[iAnchor].top -= rcClient.top;       // offset
            rgAnchorOffsets[iAnchor].bottom -= rcClient.bottom; // offset
            break;

        case AF_BOTTOM:
            rgAnchorOffsets[iAnchor].top = rgAnchorOffsets[iAnchor].bottom - rgAnchorOffsets[iAnchor].top;   // height
            rgAnchorOffsets[iAnchor].bottom -= rcClient.bottom; // offset
            break;
        }
    }
}

inline void OnSize(HWND hdlg, const ANCHOR rgAnchors[], size_t cAnchors, const RECT rgAnchorOffsets[])
{
    HDWP hdwp = BeginDeferWindowPos(static_cast<UINT>(cAnchors));
    if (hdwp)
    {
        RECT rcClient;
        GetClientRect(hdlg, &rcClient);

        for (DWORD iAnchor = 0; iAnchor < cAnchors; iAnchor++)
        {
            const HWND hwndControl = GetDlgItem(hdlg, rgAnchors[iAnchor].idControl);
            RECT rcNewPos;
            GetWindowRectInClient(hwndControl, &rcNewPos);
            const ANCHOR &anchor = rgAnchors[iAnchor];
            switch (anchor.aff & AF_LEFT_AND_RIGHT)
            {
            case AF_RIGHT:
                rcNewPos.right = rcClient.right + rgAnchorOffsets[iAnchor].right;
                rcNewPos.left = rcNewPos.right - rgAnchorOffsets[iAnchor].left; // rgAnchorOffsets[iAnchor].left contains the width of the control
                break;
            case AF_LEFT_AND_RIGHT:
                rcNewPos.right = rcClient.right + rgAnchorOffsets[iAnchor].right;
                break;
            }

            switch (anchor.aff & AF_TOP_AND_BOTTOM)
            {
            case AF_BOTTOM:
                rcNewPos.bottom = rcClient.bottom + rgAnchorOffsets[iAnchor].bottom;
                rcNewPos.top = rcNewPos.bottom - rgAnchorOffsets[iAnchor].top; // rgAnchorOffsets[iAnchor].top contains the height of the control
                break;
            case AF_TOP_AND_BOTTOM:
                rcNewPos.bottom = rcClient.bottom + rgAnchorOffsets[iAnchor].bottom;
                break;
            }

            MoveWindow(hwndControl, rcNewPos.left, rcNewPos.top, rcNewPos.right - rcNewPos.left,  rcNewPos.bottom - rcNewPos.top, TRUE);

            DeferWindowPos(hdwp, hwndControl, 0, rcNewPos.left, rcNewPos.top, rcNewPos.right - rcNewPos.left,  rcNewPos.bottom - rcNewPos.top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        EndDeferWindowPos(hdwp);
    }
}

// Make the UI look nice but using the v6 common controls
// Set up common controls v6 the easy way.  By doing this, there is no need
// to call InitCommonControlsEx().
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
