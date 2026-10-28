' FILE: webp.inc — binding FFI WebP (namespace WEBP) per il pacchetto gmbsuite 'webp'.
' Uso: REQUIRE "webp"   (oppure IMPORT WEBP)   ->  chiamate WEBP.<Membro>(...)
'
' Convenzione FFI GMBasic: firma OMOGENEA (tutti LONG), stringhe AS STRING, output scalare
' via BYREF LONG. La lib e' lo shim self-contained (libwebp statica) caricato dalla cache
' pacchetti (~/.gmb/packages/webp/<ver>/lib/<platform>/). Interscambio via PPM (P6 RGB8).

NAMESPACE WEBP

' 1 = shim disponibile.
PUBLIC DECLARE FUNCTION AVAILABLE LIB "gmbwebp" ALIAS "gmbwebp_available" () AS LONG
' Dimensioni di un WebP: scrive w/h (BYREF). 0 ok, -1 err.
PUBLIC DECLARE FUNCTION INFO LIB "gmbwebp" ALIAS "gmbwebp_info" (path AS STRING, BYREF w AS LONG, BYREF h AS LONG) AS LONG
' Decodifica WebP -> PPM (P6). 0 ok, -1 err.
PUBLIC DECLARE FUNCTION DECODEPPM LIB "gmbwebp" ALIAS "gmbwebp_decode_ppm" (webp AS STRING, ppm AS STRING) AS LONG
' Codifica PPM (P6) -> WebP lossy (quality 1..100). 0 ok, -1 err.
PUBLIC DECLARE FUNCTION ENCODEPPM LIB "gmbwebp" ALIAS "gmbwebp_encode_ppm" (ppm AS STRING, webp AS STRING, quality AS LONG) AS LONG
' Converte in base all'estensione di out (.ppm/.pgm = decode; altro = encode). 0 ok.
PUBLIC DECLARE FUNCTION CONVERT LIB "gmbwebp" ALIAS "gmbwebp_convert" (inp AS STRING, outp AS STRING, quality AS LONG) AS LONG
