' EXPECT: hsl= 0 : 0 , 1 , 0.5|hsl2= 0 : 0 , 255 , 0|cmyk= 0 : 1 , 0.498039215686275 , 0 , 0|cmyk2= 0 : 0 , 128 , 255|sat= 0 : 160 , 80 , 0
' FFI gmbgraphics: GFXCOLOR — conversioni HSL/CMYK e saturazione.
' Solo calcolo: nessun display, parità 4 backend.
IMPORT GFXCOLOR

SUB Main()
  DIM o(3) AS DOUBLE

  PRINT "hsl="; GFXCOLOR.RGB2HSL(255, 0, 0, o()); ":";
  PRINT o(0); ","; o(1); ","; o(2)
  PRINT "hsl2="; GFXCOLOR.HSL2RGB(120, 1, 0.5, o()); ":";
  PRINT o(0); ","; o(1); ","; o(2)

  PRINT "cmyk="; GFXCOLOR.RGB2CMYK(0, 128, 255, o()); ":";
  PRINT o(0); ","; o(1); ","; o(2); ","; o(3)
  PRINT "cmyk2="; GFXCOLOR.CMYK2RGB(1, 0.5, 0, 0, o()); ":";
  PRINT o(0); ","; o(1); ","; o(2)

  PRINT "sat="; GFXCOLOR.SATURATE(120, 80, 40, 0.5, o()); ":";
  PRINT o(0); ","; o(1); ","; o(2)
END SUB
