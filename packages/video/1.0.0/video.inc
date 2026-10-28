' FILE: video.inc — binding FFI Video (namespace VIDEO) per il pacchetto gmbsuite 'video'.
' Uso: REQUIRE "video"   (oppure IMPORT VIDEO)   ->  chiamate VIDEO.<Membro>(...)
'
' Convenzione FFI GMBasic: firma OMOGENEA (tutti LONG), stringhe AS STRING, output scalare
' via BYREF LONG. La lib e' lo shim self-contained (ffmpeg compilato dai sorgenti e statico).
' Output frame come PPM (P6 RGB8).

NAMESPACE VIDEO

PUBLIC DECLARE FUNCTION AVAILABLE LIB "gmbvideo" ALIAS "gmbvideo_available" () AS LONG
PUBLIC DECLARE FUNCTION INFO LIB "gmbvideo" ALIAS "gmbvideo_info" (path AS STRING, BYREF w AS LONG, BYREF h AS LONG, BYREF dur_ms AS LONG) AS LONG
PUBLIC DECLARE FUNCTION FRAME LIB "gmbvideo" ALIAS "gmbvideo_frame" (path AS STRING, t_ms AS LONG, ppm_out AS STRING) AS LONG
PUBLIC DECLARE FUNCTION FIRSTFRAMEPPM LIB "gmbvideo" ALIAS "gmbvideo_convert" (path AS STRING, ppm_out AS STRING) AS LONG
