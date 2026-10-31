' FILE: Graphics/GraphicsColorPalette.bas
' =================================================================================================
'       PALETTE RGB VIA PIXELBUFFER: gradiente R×G×B a risoluzione 320x240 (76800 pixel)
'       calcolato in un buffer e disegnato in UN colpo solo (texture) — non N LINE.
' =================================================================================================

SCREEN 12 ' 640x480

w = 320
h = 240
DIM px(76799)

FOR y = 0 TO 239
    FOR x = 0 TO 319
        px(y * 320 + x) = INT(x / 319 * 255) * 65536 + INT(y / 239 * 255) * 256 + INT(((x + y) MOD 3) * 85)
    NEXT
NEXT

PIXEL BUFFER w, h, px
PRINT
PRINT "Palette RGB da PIXEL BUFFER. Premi un tasto."
WAITKEY
