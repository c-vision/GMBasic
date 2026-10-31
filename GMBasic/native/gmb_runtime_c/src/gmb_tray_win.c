/* gmb_tray_win.c — system tray Windows via Shell_NotifyIcon (Win32, Unicode).
 * Main thread: crea una finestra nascosta che riceve i click; poll() pompa i messaggi
 * (integrabile col loop dell'host/SDL). Menu con TrackPopupMenu. */
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include "gmb_tray.h"

#define GMB_TRAY_WM      (WM_APP + 1)
#define GMB_TRAY_UID     1
#define GMB_TRAY_MAXIDS  256

static HWND   g_hwnd = NULL;
static HMENU  g_menu = NULL;
static int    g_itemCount = 0;
static int    g_ids[GMB_TRAY_MAXIDS];
static gmb_tray_cb g_cb = NULL;
static void  *g_user = NULL;
static int    g_created = 0;
static WCHAR  g_class[] = L"GmbTrayWnd";
static NOTIFYICONDATAW g_nid;

static LRESULT CALLBACK GmbTrayWndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == GMB_TRAY_WM) {
        if (LOWORD(l) == WM_RBUTTONUP || LOWORD(l) == WM_LBUTTONUP) {
            POINT p; GetCursorPos(&p);
            SetForegroundWindow(h);
            int cmd = (int)TrackPopupMenu(g_menu, TPM_RETURNCMD | TPM_NONOTIFY,
                                          p.x, p.y, 0, h, NULL);
            if (cmd && g_cb) g_cb(cmd, g_user);
            PostMessageW(h, WM_NULL, 0, 0);
        }
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static int EnsureWindow(void)
{
    if (g_hwnd) return 0;
    WNDCLASSEXW wc; ZeroMemory(&wc, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = GmbTrayWndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = g_class;
    RegisterClassExW(&wc);
    g_hwnd = CreateWindowExW(0, g_class, L"GMB Tray", 0, 0, 0, 0, 0,
                             HWND_MESSAGE, NULL, wc.hInstance, NULL);
    return g_hwnd ? 0 : -1;
}

static DWORD g_mainTid = 0;
int gmb_tray_win_available(void) { if (!g_mainTid) g_mainTid = GetCurrentThreadId(); return 1; }
int gmb_tray_win_is_main(void) { return g_mainTid == 0 || g_mainTid == GetCurrentThreadId(); }
const char *gmb_tray_win_backend(void) { return "win"; }

int gmb_tray_win_create(const char *icon_path, const char *tooltip)
{
    if (EnsureWindow() != 0) return -1;
    if (!g_menu) g_menu = CreatePopupMenu();
    ZeroMemory(&g_nid, sizeof g_nid);
    g_nid.cbSize = sizeof g_nid;
    g_nid.hWnd = g_hwnd;
    g_nid.uID = GMB_TRAY_UID;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = GMB_TRAY_WM;
    g_nid.hIcon = (icon_path && icon_path[0])
        ? (HICON)LoadImageA(NULL, icon_path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE)
        : LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    if (!g_nid.hIcon) g_nid.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    if (tooltip && tooltip[0]) MultiByteToWideChar(CP_UTF8, 0, tooltip, -1, g_nid.szTip, 128);
    g_created = Shell_NotifyIconW(NIM_ADD, &g_nid) ? 1 : 0;
    return g_created ? 0 : -1;
}

int gmb_tray_win_item(int id, const char *label)
{
    if (!g_menu || g_itemCount >= GMB_TRAY_MAXIDS) return -1;
    WCHAR wl[256];
    MultiByteToWideChar(CP_UTF8, 0, label ? label : "", -1, wl, 256);
    AppendMenuW(g_menu, MF_STRING, (UINT_PTR)id, wl);
    g_ids[g_itemCount++] = id;
    return 0;
}

int gmb_tray_win_sep(void)
{
    if (!g_menu) return -1;
    AppendMenuW(g_menu, MF_SEPARATOR, 0, NULL);
    return 0;
}

int gmb_tray_win_tooltip(const char *text)
{
    if (!g_created) return -1;
    MultiByteToWideChar(CP_UTF8, 0, text ? text : "", -1, g_nid.szTip, 128);
    g_nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    return 0;
}

void gmb_tray_win_callback(gmb_tray_cb cb, void *user) { g_cb = cb; g_user = user; }

/* Pompa i messaggi pendenti della finestra tray (thread corrente). */
int gmb_tray_win_poll(void)
{
    int n = 0; MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        n++;
    }
    return n;
}

void gmb_tray_win_remove(void)
{
    if (g_created) { Shell_NotifyIconW(NIM_DELETE, &g_nid); g_created = 0; }
    if (g_menu) { DestroyMenu(g_menu); g_menu = NULL; }
    if (g_hwnd) { DestroyWindow(g_hwnd); g_hwnd = NULL; }
    g_itemCount = 0;
}

int gmb_tray_win_selftest(void) { return g_created ? 1 : 0; }
#endif /* _WIN32 */
