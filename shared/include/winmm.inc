' FILE: include/winmm.inc
'   WINMM — shim Win32 (multimedia/timer). Libreria libs/libwinmm.dylib (§9).
'   Uso: IMPORT WINMM
NAMESPACE WINMM
PUBLIC DECLARE FUNCTION timeGetTime LIB "winmm" () AS LONG
PUBLIC DECLARE FUNCTION PlaySoundA LIB "winmm" (name AS STRING, hmod AS LONG, flags AS LONG) AS LONG
PUBLIC DECLARE FUNCTION mciSendStringA LIB "winmm" (cmd AS STRING, ret() AS LONG, retlen AS LONG, hwnd AS LONG) AS LONG
