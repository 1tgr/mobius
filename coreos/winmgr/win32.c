/* $Id$ */

#include "common.h"
#include <stdlib.h>
#include "internal.h"
#include <crtdbg.h>
#include <assert.h>

static DWORD w32_thread_param;
static HWND w32_window;
static HDC w32_dc;
static HBITMAP w32_bitmap;
static HANDLE w32_ready_event, w32_server_exited;
static CRITICAL_SECTION w32_csdc;
static RECT w32_dirty;

extern bool wmgr_wantquit;
extern lmutex_t mux_clients;
extern wmgr_client_t *client_first, *client_last;

void *WmgrGetThreadParam(void)
{
    return TlsGetValue(w32_thread_param);
}

typedef struct win32_thread_t win32_thread_t;
struct win32_thread_t
{
    HANDLE started;
    void (*entry)(void);
    void *param;
};

static unsigned int WmgrWin32ThreadEntry(void *param)
{
    win32_thread_t *thr;

    thr = (win32_thread_t*) param;
    TlsSetValue(w32_thread_param, thr->param);
    thr->entry();
    free(thr);
    return 0;
}

handle_t ThrCreateThread(void (*entry)(void), void *param, unsigned priority)
{
    win32_thread_t *thr;
    unsigned ret;

    thr = malloc(sizeof(*thr));
    if (thr == NULL)
        return 0;

    thr->entry = entry;
    thr->param = param;
    ret = _beginthread(WmgrWin32ThreadEntry, 0, thr);
    if ((int) ret == -1)
        return 0;
    else
        return (handle_t) ret;
}

static COLORREF GmgrMglColourToWin32(colour_t clr)
{
    return RGB(MGL_RED(clr), MGL_GREEN(clr), MGL_BLUE(clr));
}

void GmgrFillRect(gfx_h hgfx, const rect_t *rect)
{
    RECT t, s, r = { rect->left, rect->top, rect->right, rect->bottom };
    HBRUSH brush;
    gfx_t *gfx;
    unsigned i;

    gfx = GmgrLockGraphics(hgfx);
    if (gfx == NULL)
        return;

    brush = CreateSolidBrush(GmgrMglColourToWin32(gfx->colour_fill));
    EnterCriticalSection(&w32_csdc);

    for (i = 0; i < gfx->clip.num_rects; i++)
    {
        t.left = gfx->clip.rects[i].left;
        t.top = gfx->clip.rects[i].top;
        t.right = gfx->clip.rects[i].right;
        t.bottom = gfx->clip.rects[i].bottom;
        if (IntersectRect(&s, &r, &t))
            FillRect(w32_dc, &s, brush);
    }

    DeleteObject(brush);
    UnionRect(&w32_dirty, &w32_dirty, &r);
    LeaveCriticalSection(&w32_csdc);

    GmgrUnlockGraphics(gfx);
}

void GmgrLine(gfx_h hgfx, int x1, int y1, int x2, int y2)
{
    gfx_t *gfx;
    HPEN old_pen, pen;
    RECT r, s, t;
    unsigned i;

    gfx = GmgrLockGraphics(hgfx);
    if (gfx == NULL)
        return;

    r.left = min(x1, x2);
    r.top = min(y1, y2);
    r.right = max(x1, x2) + 1;
    r.bottom = max(y1, y2) + 1;

    EnterCriticalSection(&w32_csdc);
    pen = CreatePen(PS_SOLID, 1, GmgrMglColourToWin32(gfx->colour_pen));
    old_pen = SelectObject(w32_dc, pen);
    for (i = 0; i < gfx->clip.num_rects; i++)
    {
        t.left = gfx->clip.rects[i].left;
        t.top = gfx->clip.rects[i].top;
        t.right = gfx->clip.rects[i].right;
        t.bottom = gfx->clip.rects[i].bottom;
        if (IntersectRect(&s, &r, &t))
        {
            MoveToEx(w32_dc, s.left, s.top, NULL);
            LineTo(w32_dc, s.right, s.bottom);
        }
    }

    SelectObject(w32_dc, old_pen);

    UnionRect(&w32_dirty, &w32_dirty, &r);
    LeaveCriticalSection(&w32_csdc);
    GmgrUnlockGraphics(gfx);
}

void GmgrRectangle(gfx_h hgfx, const rect_t *rect)
{
    /*GmgrLine(hgfx, rect->left, rect->top, rect->left, rect->bottom);
    GmgrLine(hgfx, rect->left, rect->bottom, rect->right, rect->bottom);
    GmgrLine(hgfx, rect->right, rect->bottom, rect->right, rect->top);
    GmgrLine(hgfx, rect->right, rect->top, rect->left, rect->top);*/
}

void GmgrDrawText(gfx_h hgfx, const rect_t *rect, const wchar_t *str, size_t length)
{
    RECT r = { rect->left, rect->top, rect->right, rect->bottom };
    gfx_t *gfx;

    gfx = GmgrLockGraphics(hgfx);
    if (gfx == NULL)
        return;

    EnterCriticalSection(&w32_csdc);
    SetTextColor(w32_dc, GmgrMglColourToWin32(gfx->colour_pen));
    SetBkColor(w32_dc, GmgrMglColourToWin32(gfx->colour_fill));
    DrawTextW(w32_dc, str, length, &r, DT_LEFT | DT_TOP | DT_WORDBREAK);

    UnionRect(&w32_dirty, &w32_dirty, &r);
    LeaveCriticalSection(&w32_csdc);
    GmgrUnlockGraphics(gfx);
}

void GmgrFinishedPainting(gfx_h hgfx)
{
    EnterCriticalSection(&w32_csdc);
    InvalidateRect(w32_window, &w32_dirty, FALSE);
    SetRectEmpty(&w32_dirty);
    LeaveCriticalSection(&w32_csdc);
}

static LRESULT CALLBACK WmgrWin32WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static BYTE keystate[256];
    HDC dc;
    PAINTSTRUCT ps;
    MINMAXINFO *mmi;
    RECT rect;
    SCROLLINFO si;
    int old_pos, x, y, n, i;
    wchar_t wc[5];

    switch (msg)
    {
    case WM_SIZE:
        si.cbSize = sizeof(si);
        si.fMask = SIF_RANGE | SIF_PAGE;
        si.nMin = 0;
        si.nPage = 50;

        si.nMax = SCREEN_WIDTH - LOWORD(lParam);
        SetScrollInfo(hwnd, SB_HORZ, &si, FALSE);
        si.nMax = SCREEN_HEIGHT - HIWORD(lParam);
        SetScrollInfo(hwnd, SB_VERT, &si, FALSE);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_PAINT:
        EnterCriticalSection(&w32_csdc);
        dc = BeginPaint(hwnd, &ps);
        x = GetScrollPos(hwnd, SB_HORZ);
        y = GetScrollPos(hwnd, SB_VERT);
        BitBlt(dc, -x, -y, SCREEN_WIDTH, SCREEN_HEIGHT, w32_dc, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        LeaveCriticalSection(&w32_csdc);

        if (w32_ready_event != NULL)
            SetEvent(w32_ready_event);
        break;

    case WM_KEYDOWN:
        GetKeyboardState(keystate);
        n = ToUnicode(wParam, HIWORD(lParam), keystate, wc, _countof(wc), 0);
        if (n > 0)
            for (i = 0; i < n; i++)
                WmgrQueueKeyboardInput(wc[i]);
        break;

    case WM_GETMINMAXINFO:
        DefWindowProc(hwnd, msg, wParam, lParam);
        rect.left = rect.top = 0;
        rect.right = SCREEN_WIDTH;
        rect.bottom = SCREEN_HEIGHT;
        AdjustWindowRect(&rect, GetWindowLong(hwnd, GWL_STYLE), FALSE);
        mmi = (MINMAXINFO*) lParam;
        mmi->ptMaxTrackSize.x = rect.right - rect.left;
        mmi->ptMaxTrackSize.y = rect.bottom - rect.top;
        mmi->ptMaxSize = mmi->ptMaxTrackSize;
        return 0;

    case WM_MOUSEMOVE:
        WmgrQueueMouseInput(MSG_MOUSEMOVE, LOWORD(lParam), HIWORD(lParam));
        break;

    case WM_LBUTTONDOWN:
        WmgrQueueMouseInput(MSG_MOUSEDOWN, LOWORD(lParam), HIWORD(lParam));
        break;

    case WM_LBUTTONUP:
        WmgrQueueMouseInput(MSG_MOUSEUP, LOWORD(lParam), HIWORD(lParam));
        break;

    case WM_HSCROLL:
    case WM_VSCROLL:
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS;
        GetScrollInfo(hwnd, msg == WM_HSCROLL ? SB_HORZ : SB_VERT, &si);
        old_pos = si.nPos;

        switch (LOWORD(wParam))
        {
        case SB_LINELEFT:
        /*case SB_LINEUP:*/     /* xxx -- SB_LINEUP == SB_LINELEFT */
            si.nPos--;
            break;

        case SB_LINERIGHT:
        /*case SB_LINEDOWN:*/
            si.nPos++;
            break;

        case SB_PAGELEFT:
        /*case SB_PAGEUP:*/
            si.nPos -= 50;
            break;

        case SB_PAGERIGHT:
        /*case SB_PAGEDOWN:*/
            si.nPos += 50;
            break;

        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            si.nPos = HIWORD(wParam);
            break;
        }

        if (msg == WM_HSCROLL)
            ScrollWindow(hwnd, old_pos - si.nPos, 0, NULL, NULL);
        else
            ScrollWindow(hwnd, 0, old_pos - si.nPos, NULL, NULL);

        SetScrollInfo(hwnd, msg == WM_HSCROLL ? SB_HORZ : SB_VERT, &si, TRUE);
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void WmgrWin32GuiThread(void *param)
{
    wmgr_client_t *client;
    MSG msg;
    HDC screen;
    int code;
    WNDCLASS wc =
    {
        CS_OWNDC,
        WmgrWin32WndProc,
        0,
        0,
        GetModuleHandle(NULL),
        LoadIcon(NULL, IDI_APPLICATION),
        LoadCursor(NULL, IDC_ARROW),
        NULL,
        NULL,
        "WmgrWindowClass",
    };

    RegisterClass(&wc);

    w32_window = CreateWindowEx(WS_EX_OVERLAPPEDWINDOW,
        wc.lpszClassName, "The Möbius Window Manager", 
        WS_VISIBLE | WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL, 
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, wc.hInstance, NULL);

    screen = GetDC(w32_window);
    w32_dc = CreateCompatibleDC(screen);
    w32_bitmap = CreateCompatibleBitmap(screen, SCREEN_WIDTH, SCREEN_HEIGHT);
    SelectObject(w32_dc, w32_bitmap);
    SelectObject(w32_dc, GetStockObject(DEFAULT_GUI_FONT));

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DeleteDC(w32_dc);
    DeleteObject(w32_bitmap);
    ReleaseDC(w32_window, w32_dc);

    wmgr_wantquit = true;
    code = 0;
    w32_server_exited = CreateEvent(NULL, FALSE, FALSE, NULL);

    LmuxAcquire(&mux_clients);
    for (client = client_first; client != NULL; client = client->next)
        WmgrDispatchMessage(client, 0, MSG_QUIT, &code, sizeof(code));
    LmuxRelease(&mux_clients);

    WaitForSingleObject(w32_server_exited, INFINITE);
    CloseHandle(w32_server_exited);
    w32_server_exited = NULL;
    DeleteCriticalSection(&w32_csdc);
}

void WmgrWin32ServerIsExiting(void)
{
    assert(w32_server_exited != NULL);
    SetEvent(w32_server_exited);
}

void WmgrWin32Init(void)
{
    DWORD t;
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF/* | 
        _CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF*/);
    InitializeCriticalSection(&w32_csdc);
    w32_thread_param = TlsAlloc();
    w32_ready_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    _beginthread(WmgrWin32GuiThread, 0, NULL);
    t = GetTickCount();
    WaitForSingleObject(w32_ready_event, INFINITE);
    printf("[%d] ", GetTickCount() - t);
    CloseHandle(w32_ready_event);
    w32_ready_event = NULL;
}

int main(int argc, char **argv);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    return main(__argc, __argv);
}
