' FILE: include/kernel32.inc
' =================================================================================================
'   KERNEL32 — shim Win32 (sistema) per macOS/Linux
' =================================================================================================
' Esporta un ampio sottoinsieme di `kernel32.dll`: tempo, sleep/debug, file I/O, memoria, stringhe,
' modulo/env/processo. Libreria `libs/libkernel32.dylib` (symlink a `libwin32shim.dylib`), risolta
' dall'auto-discovery `libs/` (Guide/FFI-Guida-Utente.md §9). Tempi = orologio reale; I/O file =
' POSIX reale (tmp/cartella corrente); struct/registry = stub deterministici.
'
' Uso:   IMPORT KERNEL32   /   IMPORT KERNEL32 AS K32
' =================================================================================================

NAMESPACE KERNEL32

PUBLIC CONST TIME_ZONE_ID_UNKNOWN = 0
PUBLIC CONST TIME_ZONE_ID_STANDARD = 1
PUBLIC CONST TIME_ZONE_ID_DAYLIGHT = 2
PUBLIC CONST GENERIC_READ = 2147483648
PUBLIC CONST GENERIC_WRITE = 1073741824
PUBLIC CONST CREATE_NEW = 1
PUBLIC CONST CREATE_ALWAYS = 2
PUBLIC CONST OPEN_EXISTING = 3
PUBLIC CONST OPEN_ALWAYS = 4
PUBLIC CONST TRUNCATE_EXISTING = 5
PUBLIC CONST INVALID_HANDLE_VALUE = -1

' --- tempo / debug ---
PUBLIC DECLARE FUNCTION GetTickCount LIB "kernel32" () AS LONG
PUBLIC DECLARE FUNCTION GetTickCount64 LIB "kernel32" () AS LONG
PUBLIC DECLARE SUB Sleep LIB "kernel32" (ms AS LONG)
PUBLIC DECLARE SUB OutputDebugStringA LIB "kernel32" (s AS STRING)
PUBLIC DECLARE SUB OutputDebugStringW LIB "kernel32" (s AS STRING)
PUBLIC DECLARE FUNCTION Beep LIB "kernel32" (freq AS LONG, dur AS LONG) AS LONG
PUBLIC DECLARE FUNCTION QueryPerformanceCounter LIB "kernel32" (qout() AS LONG) AS LONG
PUBLIC DECLARE FUNCTION QueryPerformanceFrequency LIB "kernel32" (qout() AS LONG) AS LONG

' --- errore / processo ---
PUBLIC DECLARE FUNCTION GetLastError LIB "kernel32" () AS LONG
PUBLIC DECLARE SUB SetLastError LIB "kernel32" (e AS LONG)
PUBLIC DECLARE FUNCTION GetCurrentProcessId LIB "kernel32" () AS LONG
PUBLIC DECLARE FUNCTION GetCurrentThreadId LIB "kernel32" () AS LONG
PUBLIC DECLARE SUB ExitProcess LIB "kernel32" (code AS LONG)
PUBLIC DECLARE FUNCTION WinExec LIB "kernel32" (cmdline AS STRING, show AS LONG) AS LONG
PUBLIC DECLARE FUNCTION GetVersion LIB "kernel32" () AS LONG

' --- file I/O ---
PUBLIC DECLARE FUNCTION CreateFileA LIB "kernel32" (name AS STRING, access AS LONG, share AS LONG, sa AS LONG, disp AS LONG, attrs AS LONG, tpl AS LONG) AS LONG
PUBLIC DECLARE FUNCTION ReadFile LIB "kernel32" (h AS LONG, buf() AS LONG, nbytes AS LONG, nreadp() AS LONG, ov AS LONG) AS LONG
PUBLIC DECLARE FUNCTION WriteFile LIB "kernel32" (h AS LONG, buf() AS LONG, nbytes AS LONG, nwritep() AS LONG, ov AS LONG) AS LONG
PUBLIC DECLARE FUNCTION CloseHandle LIB "kernel32" (h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION GetFileSize LIB "kernel32" (h AS LONG, highp() AS LONG) AS LONG
PUBLIC DECLARE FUNCTION SetFilePointer LIB "kernel32" (h AS LONG, lo AS LONG, highp() AS LONG, method AS LONG) AS LONG
PUBLIC DECLARE FUNCTION DeleteFileA LIB "kernel32" (name AS STRING) AS LONG
PUBLIC DECLARE FUNCTION MoveFileA LIB "kernel32" (src AS STRING, dst AS STRING) AS LONG
PUBLIC DECLARE FUNCTION CopyFileA LIB "kernel32" (src AS STRING, dst AS STRING, failifexists AS LONG) AS LONG
PUBLIC DECLARE FUNCTION RemoveDirectoryA LIB "kernel32" (path AS STRING) AS LONG
PUBLIC DECLARE FUNCTION CreateDirectoryA LIB "kernel32" (path AS STRING, sa AS LONG) AS LONG
PUBLIC DECLARE FUNCTION GetFileAttributesA LIB "kernel32" (name AS STRING) AS LONG
PUBLIC DECLARE FUNCTION SetFileAttributesA LIB "kernel32" (name AS STRING, attrs AS LONG) AS LONG
PUBLIC DECLARE FUNCTION GetCurrentDirectoryA LIB "kernel32" (n AS LONG, buf() AS LONG) AS LONG
PUBLIC DECLARE FUNCTION SetCurrentDirectoryA LIB "kernel32" (path AS STRING) AS LONG
PUBLIC DECLARE FUNCTION FindFirstFileA LIB "kernel32" (pat AS STRING, data() AS LONG) AS LONG
PUBLIC DECLARE FUNCTION FindNextFileA LIB "kernel32" (h AS LONG, data() AS LONG) AS LONG
PUBLIC DECLARE FUNCTION FindClose LIB "kernel32" (h AS LONG) AS LONG

' --- memoria ---
PUBLIC DECLARE FUNCTION GlobalAlloc LIB "kernel32" (flags AS LONG, nbytes AS LONG) AS LONG
PUBLIC DECLARE FUNCTION GlobalFree LIB "kernel32" (p AS LONG) AS LONG
PUBLIC DECLARE FUNCTION LocalAlloc LIB "kernel32" (flags AS LONG, nbytes AS LONG) AS LONG
PUBLIC DECLARE FUNCTION LocalFree LIB "kernel32" (p AS LONG) AS LONG

' --- stringhe ---
PUBLIC DECLARE FUNCTION lstrlenA LIB "kernel32" (s AS STRING) AS LONG
PUBLIC DECLARE FUNCTION lstrcpyA LIB "kernel32" (dst() AS LONG, src AS STRING) AS LONG
PUBLIC DECLARE FUNCTION lstrcpynA LIB "kernel32" (dst() AS LONG, src AS STRING, n AS LONG) AS LONG
PUBLIC DECLARE FUNCTION lstrcatA LIB "kernel32" (dst() AS LONG, src AS STRING) AS LONG
PUBLIC DECLARE FUNCTION lstrcmpA LIB "kernel32" (a AS STRING, b AS STRING) AS LONG

' --- modulo / env / computer ---
PUBLIC DECLARE FUNCTION GetModuleFileNameA LIB "kernel32" (hmod AS LONG, buf() AS LONG, n AS LONG) AS LONG
PUBLIC DECLARE FUNCTION GetCommandLineA LIB "kernel32" () AS LONG
PUBLIC DECLARE FUNCTION GetEnvironmentVariableA LIB "kernel32" (name AS STRING, buf() AS LONG, n AS LONG) AS LONG
PUBLIC DECLARE FUNCTION SetEnvironmentVariableA LIB "kernel32" (name AS STRING, val AS STRING) AS LONG
PUBLIC DECLARE FUNCTION GetComputerNameA LIB "kernel32" (buf() AS LONG, n() AS LONG) AS LONG
PUBLIC DECLARE FUNCTION GetUserNameA LIB "kernel32" (buf() AS LONG, n() AS LONG) AS LONG

' --- tempo sistema (SYSTEMTIME = 8 WORD) ---
PUBLIC DECLARE SUB GetSystemTime LIB "kernel32" (st() AS LONG)
PUBLIC DECLARE SUB GetLocalTime LIB "kernel32" (st() AS LONG)
PUBLIC DECLARE SUB GetSystemTimeAsFileTime LIB "kernel32" (ft() AS LONG)
PUBLIC DECLARE FUNCTION GetTimeZoneInformation LIB "kernel32" (tzi() AS LONG) AS LONG
PUBLIC DECLARE FUNCTION SystemTimeToTzSpecificLocalTime LIB "kernel32" (tz() AS LONG, utc() AS LONG, loc() AS LONG) AS LONG
PUBLIC DECLARE FUNCTION TzSpecificLocalTimeToSystemTime LIB "kernel32" (tz() AS LONG, loc() AS LONG, utc() AS LONG) AS LONG
PUBLIC DECLARE FUNCTION FileTimeToSystemTime LIB "kernel32" (ft() AS LONG, st() AS LONG) AS LONG
PUBLIC DECLARE FUNCTION SystemTimeToFileTime LIB "kernel32" (st() AS LONG, ft() AS LONG) AS LONG
PUBLIC DECLARE FUNCTION MultiByteToWideChar LIB "kernel32" (cp AS LONG, flags AS LONG, winstr AS STRING, inlen AS LONG, wout() AS LONG, outlen AS LONG) AS LONG
PUBLIC DECLARE FUNCTION WideCharToMultiByte LIB "kernel32" (cp AS LONG, flags AS LONG, win() AS LONG, inlen AS LONG, wout() AS LONG, outlen AS LONG, defstr AS STRING, used() AS LONG) AS LONG
