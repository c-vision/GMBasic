' StarRating.bas — STARRATING in grafica, stelle colorate dal tema (accent = piene,
' bordo = vuote). Frecce destra/sinistra cambiano il valore quando ha il fuoco.
IMPORT WINDOWS
IMPORT CONTROLS
IMPORT GRAPHICS

SCREEN 12
FONT "font.ttf", 14
CLS 1

WINDOW THEME "light"

STARRATING 6, 4, 20, 5, 0
STARRATING 6, 8, 20, 5, 3
STARRATING 6, 12, 20, 5, 5
