' FILE: heif.inc — binding FFI HEIC/HEIF (namespace HEIF) per il pacchetto gmbsuite 'heif'.
' Uso: REQUIRE "heif"   (oppure IMPORT HEIF)   ->  chiamate HEIF.<Membro>(...)
'
' Convenzione FFI GMBasic: firma OMOGENEA (tutti LONG), stringhe AS STRING, output scalare
' via BYREF LONG. La lib e' lo shim self-contained (libheif+libde265+x265 compilate dai
' sorgenti e statiche) caricato dalla cache. Interscambio via PPM (P6 RGB8).

NAMESPACE HEIF

PUBLIC DECLARE FUNCTION AVAILABLE LIB "gmbheif" ALIAS "gmbheif_available" () AS LONG
PUBLIC DECLARE FUNCTION INFO LIB "gmbheif" ALIAS "gmbheif_info" (path AS STRING, BYREF w AS LONG, BYREF h AS LONG) AS LONG
PUBLIC DECLARE FUNCTION DECODEPPM LIB "gmbheif" ALIAS "gmbheif_decode_ppm" (heic AS STRING, ppm AS STRING) AS LONG
PUBLIC DECLARE FUNCTION ENCODEPPM LIB "gmbheif" ALIAS "gmbheif_encode_ppm" (ppm AS STRING, heic AS STRING, quality AS LONG) AS LONG
PUBLIC DECLARE FUNCTION CONVERT LIB "gmbheif" ALIAS "gmbheif_convert" (inp AS STRING, outp AS STRING, quality AS LONG) AS LONG
