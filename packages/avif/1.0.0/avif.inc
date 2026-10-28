' FILE: avif.inc — binding FFI AVIF (namespace AVIF) per il pacchetto gmbsuite 'avif'.
' Uso: REQUIRE "avif"   (oppure IMPORT AVIF)   ->  chiamate AVIF.<Membro>(...)
'
' Convenzione FFI GMBasic: firma OMOGENEA (tutti LONG), stringhe AS STRING, output scalare
' via BYREF LONG. La lib e' lo shim self-contained (libavif compilata dai sorgenti + libaom/
' libvmaf statiche) caricato dalla cache pacchetti. Interscambio via PPM (P6 RGB8).

NAMESPACE AVIF

' 1 = shim disponibile.
PUBLIC DECLARE FUNCTION AVAILABLE LIB "gmbavif" ALIAS "gmbavif_available" () AS LONG
' Dimensioni di un AVIF: scrive w/h (BYREF). 0 ok, -1 err.
PUBLIC DECLARE FUNCTION INFO LIB "gmbavif" ALIAS "gmbavif_info" (path AS STRING, BYREF w AS LONG, BYREF h AS LONG) AS LONG
' Decodifica AVIF -> PPM (P6). 0 ok, -1 err.
PUBLIC DECLARE FUNCTION DECODEPPM LIB "gmbavif" ALIAS "gmbavif_decode_ppm" (avif AS STRING, ppm AS STRING) AS LONG
' Codifica PPM (P6) -> AVIF (quality 1..100). 0 ok, -1 err.
PUBLIC DECLARE FUNCTION ENCODEPPM LIB "gmbavif" ALIAS "gmbavif_encode_ppm" (ppm AS STRING, avif AS STRING, quality AS LONG) AS LONG
' Converte in base all'estensione di out (.ppm/.pgm = decode; altro = encode). 0 ok.
PUBLIC DECLARE FUNCTION CONVERT LIB "gmbavif" ALIAS "gmbavif_convert" (inp AS STRING, outp AS STRING, quality AS LONG) AS LONG
