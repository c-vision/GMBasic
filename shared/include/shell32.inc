' FILE: include/shell32.inc
' =================================================================================================
'   SHELL32 — shim Win32 (ShellExecute) per macOS/Linux
' =================================================================================================
' Esporta i simboli di `shell32.dll` usati dagli EA MQL. Libreria `libs/libshell32.dylib`
' (symlink a `libwin32shim.dylib`). ShellExecute è NO-OP (ritorna 0): nessuna esecuzione
' esterna nel backtest.
'
' Uso:   IMPORT SHELL32   /   IMPORT SHELL32 AS SH
' =================================================================================================

NAMESPACE SHELL32

PUBLIC CONST SW_SHOWNORMAL = 1
PUBLIC CONST SW_HIDE = 0

PUBLIC DECLARE FUNCTION ShellExecuteA LIB "shell32" (hwnd AS LONG, op AS STRING, fname AS STRING, prms AS STRING, dir AS STRING, show AS LONG) AS LONG
PUBLIC DECLARE FUNCTION ShellExecuteW LIB "shell32" (hwnd AS LONG, op AS STRING, fname AS STRING, prms AS STRING, dir AS STRING, show AS LONG) AS LONG
PUBLIC DECLARE FUNCTION ShellExecuteExA LIB "shell32" (sei() AS LONG) AS LONG
