' FILE: Security/UploadValidationDemo.bas
' =================================================================================================
'       SECURITY — validazione upload (HTTP.SAFEFILENAME / HTTP.DETECTCONTENTTYPE)
' =================================================================================================
' Helper puro (core CommonLib.Http.FileUpload): nome file sicuro (strip path/'..'/caratteri
' pericolosi) e sniff magic-number (PNG/JPEG/GIF/PDF/ZIP/GZIP). Da usare con i FileField dei
' form multipart (req.ParseMultipartForm). Eseguibile su IL, PCodeVM e VM/C
' (DETECTCONTENTTYPE: gap VM/C dichiarato 2026-09-02).

IMPORT HTTP

PRINT "=== Upload validation ==="

PRINT "safe: "; HTTP.SAFEFILENAME("../../etc/passwd.png")
PRINT "safe: "; HTTP.SAFEFILENAME("C:\temp\..\x.bin")
PRINT "safe: "; HTTP.SAFEFILENAME("report finale.PDF")

IF HTTP.SAFEFILENAME("../../etc/a.png") = "a.png" THEN
    PRINT "SAFE NAME OK"
ELSE
    PRINT "SAFE NAME FAIL"
END IF

PRINT "type: "; HTTP.DETECTCONTENTTYPE("a")   ' nessun magic -> "" (stringa non binaria)

PRINT "UploadValidation demo done"
