' FILE: include/wininet.inc
' =================================================================================================
'   WININET — shim Win32 (HTTP) per macOS/Linux
' =================================================================================================
' Esporta i simboli di `wininet.dll` usati dagli EA MQL importati. La libreria è
' `libs/libwininet.dylib` (symlink a `libwin32shim.dylib`), risolta dall'auto-discovery
' `libs/` (Guide/FFI-Guida-Utente.md §9). Le shim sono STUB deterministici (nessuna rete):
' gli handle sono fittizi (1) e le letture ritornano EOF — sufficiente per il backtest.
'
' Uso:   IMPORT WININET          (simboli non qualificati)
'        IMPORT WININET AS W     (simboli qualificati  W.InternetOpenA)
' =================================================================================================

NAMESPACE WININET

PUBLIC CONST INTERNET_OPEN_TYPE_PRECONFIG = 0
PUBLIC CONST INTERNET_OPEN_TYPE_DIRECT = 1
PUBLIC CONST INTERNET_OPEN_TYPE_PROXY = 3
PUBLIC CONST INTERNET_FLAG_RELOAD = 2147483648
PUBLIC CONST INTERNET_FLAG_NO_CACHE_WRITE = 67108864

PUBLIC DECLARE NATIVE FUNCTION InternetOpenA LIB "wininet" (agent AS STRING, dwType AS LONG, proxy AS STRING, bypass AS STRING, flags AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION InternetOpenW LIB "wininet" (agent AS STRING, dwType AS LONG, proxy AS STRING, bypass AS STRING, flags AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION InternetOpenUrlA LIB "wininet" (h AS LONG, url AS STRING, headers AS STRING, hlen AS LONG, flags AS LONG, ctx AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION InternetOpenUrlW LIB "wininet" (h AS LONG, url AS STRING, headers AS STRING, hlen AS LONG, flags AS LONG, ctx AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION InternetConnectA LIB "wininet" (h AS LONG, server AS STRING, port AS LONG, user AS STRING, pass AS STRING, service AS LONG, flags AS LONG, ctx AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION InternetConnectW LIB "wininet" (h AS LONG, server AS STRING, port AS LONG, user AS STRING, pass AS STRING, service AS LONG, flags AS LONG, ctx AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION InternetCloseHandle LIB "wininet" (h AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION InternetAttemptConnect LIB "wininet" (reserved AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION InternetCanonicalizeUrlA LIB "wininet" (url AS STRING, buf() AS LONG, blen() AS LONG, flags AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION HttpOpenRequestA LIB "wininet" (h AS LONG, verb AS STRING, obj AS STRING, ver AS STRING, referer AS STRING, accept AS STRING, flags AS LONG, ctx AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION HttpAddRequestHeadersA LIB "wininet" (h AS LONG, headers AS STRING, hlen AS LONG, flags AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION HttpSendRequestA LIB "wininet" (h AS LONG, headers AS STRING, hlen AS LONG, opt() AS LONG, optlen AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION HttpQueryInfoA LIB "wininet" (h AS LONG, level AS LONG, buf() AS LONG, blen() AS LONG, idx() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION InternetReadFile LIB "wininet" (h AS LONG, buf() AS LONG, toread AS LONG, nread() AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION InternetSetOptionA LIB "wininet" (h AS LONG, opt AS LONG, buf() AS LONG, blen AS LONG) AS LONG
PUBLIC DECLARE NATIVE FUNCTION InternetQueryOptionA LIB "wininet" (h AS LONG, opt AS LONG, buf() AS LONG, blen() AS LONG) AS LONG
