' gfxcodec.inc — namespace GFXCODEC via FFI (libgmbgraphics: gmbsuite/gmbgraphics).
' BC1/DXT1 (compressione texture GPU). Byte come DOUBLE 0..255.
' Blocco = 4x4 RGB = 48 byte in ingresso / 8 byte compressi.
NAMESPACE GFXCODEC

' Comprimi un blocco 4x4: rgb() 48 valori -> out() 8 byte.
DECLARE FUNCTION BC1ENCODEBLOCK LIB "gmbgraphics" ALIAS "ofx_gfx_bc1_encode_block" (rgb() AS DOUBLE, out() AS DOUBLE) AS LONG
' Decomprimi un blocco: in() 8 byte -> out() 48 valori RGB.
DECLARE FUNCTION BC1DECODEBLOCK LIB "gmbgraphics" ALIAS "ofx_gfx_bc1_decode_block" (in() AS DOUBLE, out() AS DOUBLE) AS LONG
' Immagine intera (w,h multipli di 4): out() = (w/4)*(h/4)*8 byte; ritorno = byte scritti o -1.
DECLARE FUNCTION BC1ENCODE LIB "gmbgraphics" ALIAS "ofx_gfx_bc1_encode" (rgb() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG
' Decodifica: in() = blocchi compressi, out() = w*h*3 valori RGB.
DECLARE FUNCTION BC1DECODE LIB "gmbgraphics" ALIAS "ofx_gfx_bc1_decode" (in() AS DOUBLE, w AS LONG, h AS LONG, out() AS DOUBLE) AS LONG
