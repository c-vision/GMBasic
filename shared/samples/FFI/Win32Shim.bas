' FILE: samples/Community/FFI/Win32Shim.bas
' =================================================================================================
'   WIN32 SHIM — API Windows "replicate" su macOS/Linux (porting di codice Windows/MQL)
' =================================================================================================
' Le librerie `wininet`/`kernel32`/`user32`/`shell32`/`winmm`/`ws2_32`/`gdi32` sono fornite dallo
' shim `libs/libwin32shim.dylib` (symlink per-libreria). Gli `.inc` stanno in `include/` e si
' importano con IMPORT. Nessun DLL Windows richiesto.
'
' Prima:  native/win32shim/build.sh   -> libs/libwin32shim.dylib (+ symlink)
' Guida:  Guide/FFI-Guida-Utente.md §9
' =================================================================================================

IMPORT WININET
IMPORT KERNEL32
IMPORT USER32
IMPORT SHELL32
IMPORT WINMM
IMPORT WS2_32
IMPORT GDI32

SUB Main()
    DIM h AS LONG

    ' --- wininet ---
    h = InternetOpenA("gmb-win32shim", INTERNET_OPEN_TYPE_DIRECT, "", "", 0)
    PRINT "InternetOpenA         = "; h
    PRINT "InternetOpenUrlA      = "; InternetOpenUrlA(h, "http://example.com", "", 0, 0, 0)
    PRINT "InternetCloseHandle   = "; InternetCloseHandle(h)
    PRINT "InternetAttemptConnect= "; InternetAttemptConnect(0)

    ' --- kernel32: tempo/errore ---
    PRINT "GetTickCount >= 0     = "; (GetTickCount() >= 0)
    PRINT "lstrlenA('hello')     = "; lstrlenA("hello")
    PRINT "GetVersion            = "; GetVersion()
    PRINT "GetCurrentProcessId>0 = "; (GetCurrentProcessId() > 0)

    ' --- kernel32: file I/O reale (POSIX) ---
    h = CreateFileA("/tmp/gmb_win32shim_test.txt", GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 128, 0)
    PRINT "CreateFileA(write) ok = "; (h <> INVALID_HANDLE_VALUE)
    PRINT "CloseHandle(write)    = "; CloseHandle(h)
    PRINT "DeleteFileA           = "; DeleteFileA("/tmp/gmb_win32shim_test.txt")

    ' --- user32 / shell32 ---
    PRINT "MessageBoxA           = "; MessageBoxA(0, "Ciao dallo shim Win32", "GMBasic", MB_OK)
    PRINT "ShellExecuteA         = "; ShellExecuteA(0, "open", "nonexistent.txt", "", "", SW_SHOWNORMAL)
    PRINT "GetDC                 = "; GetDC(0)

    ' --- winmm / ws2_32 / gdi32 ---
    PRINT "timeGetTime >= 0      = "; (timeGetTime() >= 0)
    PRINT "htons(1)              = "; htons(1)
    PRINT "WSACleanup            = "; WSACleanup()
    PRINT "GetStockObject        = "; GetStockObject(0)

    OutputDebugStringA("OutputDebugStringA via shim")
    Sleep(1)

    PRINT "Test Win32Shim completato."
END SUB
