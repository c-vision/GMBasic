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
PUBLIC DECLARE NATIVE FUNCTION GetTickCount LIB "kernel32" () AS LONG
PUBLIC DECLARE NATIVE FUNCTION GetTickCount64 LIB "kernel32" () AS LONG
PUBLIC DECLARE NATIVE SUB Sleep LIB "kernel32" (ms AS LONG)
PUBLIC DECLARE NATIVE SUB OutputDebugStringA LIB "kernel32" (s AS STRING)
PUBLIC DECLARE NATIVE SUB OutputDebugStringW LIB "kernel32" (s AS STRING)
PUBLIC DECLARE NATIVE FUNCTION Beep LIB "kernel32" (freq AS LONG, dur AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION QueryPerformanceCounter LIB "kernel32" (qout() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION QueryPerformanceFrequency LIB "kernel32" (qout() AS LONG) AS LONG

' --- errore / processo ---
PUBLIC DECLARE NATIVE FUNCTION GetLastError LIB "kernel32" () AS LONG
PUBLIC DECLARE NATIVE SUB SetLastError LIB "kernel32" (e AS LONG)
PUBLIC DECLARE NATIVE FUNCTION GetCurrentProcessId LIB "kernel32" () AS LONG
PUBLIC DECLARE NATIVE FUNCTION GetCurrentThreadId LIB "kernel32" () AS LONG
PUBLIC DECLARE NATIVE SUB ExitProcess LIB "kernel32" (code AS LONG)
PUBLIC DECLARE NATIVE FUNCTION WinExec LIB "kernel32" (cmdline AS STRING, show AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION GetVersion LIB "kernel32" () AS LONG

' --- file I/O ---
PUBLIC DECLARE NATIVE FUNCTION CreateFileA LIB "kernel32" (name AS STRING, access AS LONG, share AS LONG, sa AS LONG, disp AS LONG, attrs AS LONG, tpl AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION ReadFile LIB "kernel32" (h AS LONG, buf() AS LONG, nbytes AS LONG, nreadp() AS LONG, ov AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION WriteFile LIB "kernel32" (h AS LONG, buf() AS LONG, nbytes AS LONG, nwritep() AS LONG, ov AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION CloseHandle LIB "kernel32" (h AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION GetFileSize LIB "kernel32" (h AS LONG, highp() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION SetFilePointer LIB "kernel32" (h AS LONG, lo AS LONG, highp() AS LONG, method AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION DeleteFileA LIB "kernel32" (name AS STRING) AS LONG
PUBLIC DECLARE NATIVE FUNCTION MoveFileA LIB "kernel32" (src AS STRING, dst AS STRING) AS LONG
PUBLIC DECLARE NATIVE FUNCTION CopyFileA LIB "kernel32" (src AS STRING, dst AS STRING, failifexists AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION RemoveDirectoryA LIB "kernel32" (path AS STRING) AS LONG
PUBLIC DECLARE NATIVE FUNCTION CreateDirectoryA LIB "kernel32" (path AS STRING, sa AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION GetFileAttributesA LIB "kernel32" (name AS STRING) AS LONG
PUBLIC DECLARE NATIVE FUNCTION SetFileAttributesA LIB "kernel32" (name AS STRING, attrs AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION GetCurrentDirectoryA LIB "kernel32" (n AS LONG, buf() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION SetCurrentDirectoryA LIB "kernel32" (path AS STRING) AS LONG
PUBLIC DECLARE NATIVE FUNCTION FindFirstFileA LIB "kernel32" (pat AS STRING, data() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION FindNextFileA LIB "kernel32" (h AS LONG, data() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION FindClose LIB "kernel32" (h AS LONG) AS LONG

' --- memoria ---
PUBLIC DECLARE NATIVE FUNCTION GlobalAlloc LIB "kernel32" (flags AS LONG, nbytes AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION GlobalFree LIB "kernel32" (p AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION LocalAlloc LIB "kernel32" (flags AS LONG, nbytes AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION LocalFree LIB "kernel32" (p AS LONG) AS LONG

' --- stringhe ---
PUBLIC DECLARE NATIVE FUNCTION lstrlenA LIB "kernel32" (s AS STRING) AS LONG
PUBLIC DECLARE NATIVE FUNCTION lstrcpyA LIB "kernel32" (dst() AS LONG, src AS STRING) AS LONG
PUBLIC DECLARE NATIVE FUNCTION lstrcpynA LIB "kernel32" (dst() AS LONG, src AS STRING, n AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION lstrcatA LIB "kernel32" (dst() AS LONG, src AS STRING) AS LONG
PUBLIC DECLARE NATIVE FUNCTION lstrcmpA LIB "kernel32" (a AS STRING, b AS STRING) AS LONG

' --- modulo / env / computer ---
PUBLIC DECLARE NATIVE FUNCTION GetModuleFileNameA LIB "kernel32" (hmod AS LONG, buf() AS LONG, n AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION GetCommandLineA LIB "kernel32" () AS LONG
PUBLIC DECLARE NATIVE FUNCTION GetEnvironmentVariableA LIB "kernel32" (name AS STRING, buf() AS LONG, n AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION SetEnvironmentVariableA LIB "kernel32" (name AS STRING, val AS STRING) AS LONG
PUBLIC DECLARE NATIVE FUNCTION GetComputerNameA LIB "kernel32" (buf() AS LONG, n() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION GetUserNameA LIB "kernel32" (buf() AS LONG, n() AS LONG) AS LONG

' --- tempo sistema (SYSTEMTIME = 8 WORD) ---
PUBLIC DECLARE NATIVE SUB GetSystemTime LIB "kernel32" (st() AS LONG)
PUBLIC DECLARE NATIVE SUB GetLocalTime LIB "kernel32" (st() AS LONG)
PUBLIC DECLARE NATIVE SUB GetSystemTimeAsFileTime LIB "kernel32" (ft() AS LONG)
PUBLIC DECLARE NATIVE FUNCTION GetTimeZoneInformation LIB "kernel32" (tzi() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION SystemTimeToTzSpecificLocalTime LIB "kernel32" (tz() AS LONG, utc() AS LONG, loc() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION TzSpecificLocalTimeToSystemTime LIB "kernel32" (tz() AS LONG, loc() AS LONG, utc() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION FileTimeToSystemTime LIB "kernel32" (ft() AS LONG, st() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION SystemTimeToFileTime LIB "kernel32" (st() AS LONG, ft() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION MultiByteToWideChar LIB "kernel32" (cp AS LONG, flags AS LONG, winstr AS STRING, inlen AS LONG, wout() AS LONG, outlen AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION WideCharToMultiByte LIB "kernel32" (cp AS LONG, flags AS LONG, win() AS LONG, inlen AS LONG, wout() AS LONG, outlen AS LONG, defstr AS STRING, used() AS LONG) AS LONG
