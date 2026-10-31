' MediaImageDemo.bas — namespace MEDIA (gmbmedia): lettura/scrittura/operazioni immagini.
' Mostra l'uso FFI della libreria builtin `media` (libs/media). Nessuna dipendenza SDL.
IMPORT MEDIA WITH ALIAS

SUB Main()
    PRINT "available = " + STR(MEDIA.AVAILABLE())
    PRINT "formats   = " + MEDIA.IMGFORMATS()

    h = MEDIA.IMGREAD("player.png")
    IF h < 1 THEN
        PRINT "ERRORE lettura immagine"
        EXIT SUB
    END IF
    PRINT "read: handle=" + STR(h) + " " + STR(MEDIA.IMGW(h)) + "x" + STR(MEDIA.IMGH(h)) + " canali=" + STR(MEDIA.IMGCHANNELS(h))
    PRINT "pixel(0,0) = &H" + HEX(MEDIA.IMGGET(h, 0, 0)) + "  alpha=" + STR(MEDIA.IMGGETA(h, 0, 0))

    ' operazioni -> nuovi handle
    g = MEDIA.IMGGRAY(h)
    PRINT "gray: canali=" + STR(MEDIA.IMGCHANNELS(g))

    r = MEDIA.IMGRESIZE(h, 64, 64, 1)
    PRINT "resize(64x64,bilinear): " + STR(MEDIA.IMGW(r)) + "x" + STR(MEDIA.IMGH(r))

    c = MEDIA.IMGCROP(h, 4, 4, 16, 16)
    PRINT "crop(4,4,16,16): " + STR(MEDIA.IMGW(c)) + "x" + STR(MEDIA.IMGH(c))

    b = MEDIA.IMGBLUR(h, 2, 1)
    s = MEDIA.IMGSOBEL(h)
    PRINT "blur/sobel ok"

    ' salvataggi (output in work/, come da protocollo samples)
    ok1 = MEDIA.IMGWRITE(r, "work/out_resized.png")
    ok2 = MEDIA.IMGWRITEQ(g, "work/out_gray.jpg", 90)
    PRINT "write png=" + STR(ok1) + "  write jpg=" + STR(ok2)

    ' conversione diretta file->file
    PRINT "convert png->bmp = " + STR(MEDIA.IMGCONVERT("player.png", "work/out_player.bmp"))

    MEDIA.IMGFREE(h) : MEDIA.IMGFREE(g) : MEDIA.IMGFREE(r) : MEDIA.IMGFREE(c)
    MEDIA.IMGFREE(b) : MEDIA.IMGFREE(s)
    PRINT "done"
END SUB
