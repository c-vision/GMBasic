' gfxcolor.inc — namespace GFXCOLOR via FFI (libgmbgraphics: gmbsuite/gmbgraphics).
' Colori in componenti 0..255; H in [0,360); S/V/L in [0,1]. out() riceve i risultati:
' conversioni -> 3 componenti (0..255), HSV/HSL -> h,s,v|h,s,l, CMYK -> 4 (0..1).
' Ritorno: 0 = ok, -1 = argomenti/out non validi.
NAMESPACE GFXCOLOR

' RGB (0..255) -> HSV: out(0)=h [0,360), out(1)=s, out(2)=v [0,1].
DECLARE FUNCTION RGB2HSV LIB "gmbgraphics" ALIAS "ofx_gfx_rgb2hsv" (r AS DOUBLE, g AS DOUBLE, b AS DOUBLE, out() AS DOUBLE) AS LONG
' HSV -> RGB (0..255) in out().
DECLARE FUNCTION HSV2RGB LIB "gmbgraphics" ALIAS "ofx_gfx_hsv2rgb" (h AS DOUBLE, s AS DOUBLE, v AS DOUBLE, out() AS DOUBLE) AS LONG
' RGB -> HSL: out(0)=h, out(1)=s, out(2)=l.
DECLARE FUNCTION RGB2HSL LIB "gmbgraphics" ALIAS "ofx_gfx_rgb2hsl" (r AS DOUBLE, g AS DOUBLE, b AS DOUBLE, out() AS DOUBLE) AS LONG
' HSL -> RGB (0..255) in out().
DECLARE FUNCTION HSL2RGB LIB "gmbgraphics" ALIAS "ofx_gfx_hsl2rgb" (h AS DOUBLE, s AS DOUBLE, l AS DOUBLE, out() AS DOUBLE) AS LONG
' RGB -> CMYK: out(0..3)=c,m,y,k in [0,1].
DECLARE FUNCTION RGB2CMYK LIB "gmbgraphics" ALIAS "ofx_gfx_rgb2cmyk" (r AS DOUBLE, g AS DOUBLE, b AS DOUBLE, out() AS DOUBLE) AS LONG
' CMYK (0..1) -> RGB (0..255) in out().
DECLARE FUNCTION CMYK2RGB LIB "gmbgraphics" ALIAS "ofx_gfx_cmyk2rgb" (c AS DOUBLE, m AS DOUBLE, y AS DOUBLE, k AS DOUBLE, out() AS DOUBLE) AS LONG
' Interpolazione lineare tra due RGB: t clampato [0,1]; out() = 3 componenti 0..255.
DECLARE FUNCTION MIX LIB "gmbgraphics" ALIAS "ofx_gfx_mix" (r1 AS DOUBLE, g1 AS DOUBLE, b1 AS DOUBLE, r2 AS DOUBLE, g2 AS DOUBLE, b2 AS DOUBLE, t AS DOUBLE, out() AS DOUBLE) AS LONG
' Luminosità HSL +amt (0..1); out() = RGB 0..255.
DECLARE FUNCTION LIGHTEN LIB "gmbgraphics" ALIAS "ofx_gfx_lighten" (r AS DOUBLE, g AS DOUBLE, b AS DOUBLE, amt AS DOUBLE, out() AS DOUBLE) AS LONG
' Luminosità HSL -amt; out() = RGB 0..255.
DECLARE FUNCTION DARKEN LIB "gmbgraphics" ALIAS "ofx_gfx_darken" (r AS DOUBLE, g AS DOUBLE, b AS DOUBLE, amt AS DOUBLE, out() AS DOUBLE) AS LONG
' Saturazione HSL +amt; out() = RGB 0..255.
DECLARE FUNCTION SATURATE LIB "gmbgraphics" ALIAS "ofx_gfx_saturate" (r AS DOUBLE, g AS DOUBLE, b AS DOUBLE, amt AS DOUBLE, out() AS DOUBLE) AS LONG

' Colormap jet-like: t in [0,1] -> out() = RGB 0..255 (blu -> ciano -> verde -> giallo -> rosso).
DECLARE FUNCTION COLORMAP LIB "gmbgraphics" ALIAS "ofx_gfx_colormap" (t AS DOUBLE, out() AS DOUBLE) AS LONG
