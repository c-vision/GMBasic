' ProgressBars.bas — PROGRESS in una pagina grafica, stile guidato dal tema.
' Il valore (4o argomento) arriva al widget: 25 / 60 / 100 percento.
' WINDOW THEME cambia look&feel: "light", "dracula", "turbo", "nord", "qbasic", ...
IMPORT WINDOWS
IMPORT CONTROLS
IMPORT GRAPHICS

SCREEN 12
FONT "font.ttf", 14
CLS 1

WINDOW THEME "light"

PROGRESS 60, 50, 380, 25
PROGRESS 60, 100, 380, 60
PROGRESS 60, 150, 380, 100
