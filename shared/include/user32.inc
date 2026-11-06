' FILE: include/user32.inc
' =================================================================================================
'   USER32 — shim Win32 (GUI/messaggi) per macOS/Linux
' =================================================================================================
' Esporta i simboli di `user32.dll` usati dagli EA MQL. Libreria `libs/libuser32.dylib`
' (symlink a `libwin32shim.dylib`). MessageBox stampa su stderr; le finestre sono NO-OP
' deterministiche (gli EA che disegnano pannelli non hanno effetto nel backtest).
'
' Uso:   IMPORT USER32   /   IMPORT USER32 AS U32
' =================================================================================================

NAMESPACE USER32

PUBLIC CONST MB_OK = 0
PUBLIC CONST MB_ICONINFORMATION = 64
PUBLIC CONST SW_SHOW = 5
PUBLIC CONST WM_USER = 1024

PUBLIC DECLARE NATIVE FUNCTION MessageBoxA LIB "user32" (hwnd AS LONG, txt AS STRING, cap AS STRING, mtype AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION MessageBoxW LIB "user32" (hwnd AS LONG, txt AS STRING, cap AS STRING, mtype AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION MessageBoxExA LIB "user32" (hwnd AS LONG, txt AS STRING, cap AS STRING, mtype AS LONG, lang AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION MessageBeep LIB "user32" (mtype AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION PostMessageA LIB "user32" (hwnd AS LONG, msg AS LONG, w AS LONG, l AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION PostMessageW LIB "user32" (hwnd AS LONG, msg AS LONG, w AS LONG, l AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION SendMessageA LIB "user32" (hwnd AS LONG, msg AS LONG, w AS LONG, l AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION SendNotifyMessageA LIB "user32" (hwnd AS LONG, msg AS LONG, w AS LONG, l AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION RegisterWindowMessageA LIB "user32" (s AS STRING) AS LONG
PUBLIC DECLARE NATIVE FUNCTION GetClientRect LIB "user32" (hwnd AS LONG, rect() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION GetWindowRect LIB "user32" (hwnd AS LONG, rect() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION GetForegroundWindow LIB "user32" () AS LONG
PUBLIC DECLARE NATIVE FUNCTION FindWindowA LIB "user32" (wndcls AS STRING, wndname AS STRING) AS LONG
PUBLIC DECLARE NATIVE FUNCTION ShowWindow LIB "user32" (hwnd AS LONG, cmd AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION UpdateWindow LIB "user32" (hwnd AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION SetWindowTextA LIB "user32" (hwnd AS LONG, s AS STRING) AS LONG
PUBLIC DECLARE NATIVE FUNCTION SetWindowPos LIB "user32" (hwnd AS LONG, after AS LONG, x AS LONG, y AS LONG, cx AS LONG, cy AS LONG, flags AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION MoveWindow LIB "user32" (hwnd AS LONG, x AS LONG, y AS LONG, w AS LONG, hh AS LONG, repaint AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION IsWindowVisible LIB "user32" (hwnd AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION IsZoomed LIB "user32" (hwnd AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION GetSystemMetrics LIB "user32" (idx AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION SwapMouseButton LIB "user32" (f AS LONG) AS LONG
PUBLIC DECLARE NATIVE SUB mouse_event LIB "user32" (flags AS LONG, dx AS LONG, dy AS LONG, mdata AS LONG, extra AS LONG)
PUBLIC DECLARE NATIVE SUB keybd_event LIB "user32" (vk AS LONG, scan AS LONG, flags AS LONG, extra AS LONG)

' --- base GUI/DC ---
PUBLIC DECLARE NATIVE FUNCTION GetDC LIB "user32" (hwnd AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION ReleaseDC LIB "user32" (hwnd AS LONG, hdc AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION GetWindowLongA LIB "user32" (hwnd AS LONG, idx AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION SetWindowLongA LIB "user32" (hwnd AS LONG, idx AS LONG, v AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION DestroyWindow LIB "user32" (hwnd AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION LoadCursorA LIB "user32" (inst AS LONG, cur AS STRING) AS LONG
PUBLIC DECLARE NATIVE FUNCTION SetCursor LIB "user32" (c AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION GetKeyboardState LIB "user32" (ks() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION ScreenToClient LIB "user32" (hwnd AS LONG, pt() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION ClientToScreen LIB "user32" (hwnd AS LONG, pt() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION InvalidateRect LIB "user32" (hwnd AS LONG, r() AS LONG, erase AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION DefWindowProcA LIB "user32" (hwnd AS LONG, msg AS LONG, w AS LONG, l AS LONG) AS LONG
