IMPORT GRAPHICS
' FILE: Graphics/GraphicsPalette.bas
' =================================================================================================
'       GUIDA TEORICA: PALETTE USE/DEFINE/REPLACE/SAVE/LOAD IN MODALITA' GRAFICA
' =================================================================================================
'
' A differenza della console testuale, qui PALETTE agisce sulla superficie di disegno pixel
' (PSET/CIRCLE/LINE/...): GraphicsManager mantiene una propria copia della palette attiva,
' sincronizzata con quella della console ogni volta che PALETTE USE/DEFINE/REPLACE/LOAD viene
' eseguito mentre una modalita' grafica e' aperta (SCREEN n). I pixel gia' disegnati non
' cambiano colore retroattivamente (non c'e' un buffer indicizzato per i pixel come per il
' testo), ma tutti i disegni successivi (PSET/CIRCLE/LINE) useranno i nuovi colori.
'
' =================================================================================================

SCREEN 12
FONT "font.ttf", 12
CLS 0

CPRINT "PALETTE test (modalita' grafica)"

PALETTE USE "Console"
PALETTE 5, 255, 200, 0

DIM arr(15)
arr(0) = 0xFF0000: arr(1) = 0x00FF00: arr(2) = 0x0000FF
arr(3) = 0xFFFF00: arr(4) = 0xFF00FF: arr(5) = 0x00FFFF
arr(6) = 0x800000: arr(7) = 0x008000: arr(8) = 0x000080
arr(9) = 0x808000: arr(10) = 0x800080: arr(11) = 0x008080
arr(12) = 0xC0C0C0: arr(13) = 0x808080: arr(14) = 0xFF8800
arr(15) = 0xFFFFFF
PALETTE DEFINE "MyPal", arr()
PALETTE USE "MyPal"

' Disegna un pixel con l'indice 1 della palette attiva ("MyPal" -> verde 0x00FF00).
PSET (10, 10), 1

PALETTE SAVE "pal_test.json"

DIM arr2(15)
arr2(0) = 0x000000: arr2(1) = 0x000000
arr2(2) = 0x000000: arr2(3) = 0x000000
arr2(4) = 0x000000: arr2(5) = 0x000000
arr2(6) = 0x000000: arr2(7) = 0x000000
arr2(8) = 0x000000: arr2(9) = 0x000000
arr2(10) = 0x000000: arr2(11) = 0x000000
arr2(12) = 0x000000: arr2(13) = 0x000000
arr2(14) = 0x000000: arr2(15) = 0xFFFFFF
PALETTE REPLACE "MyPal", arr2()

' Da questo momento in poi l'indice 1 e' nero: un secondo PSET con lo stesso indice
' produce un colore diverso dal primo, a dimostrazione che REPLACE si riflette subito
' sulla superficie grafica quando la palette sostituita e' quella attiva.
PSET (20, 10), 1

PALETTE LOAD "pal_test.json"
PALETTE USE "Nord"

CPRINT "DONE"
END
