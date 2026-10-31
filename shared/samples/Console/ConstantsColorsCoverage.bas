' FILE: Console/ConstantsColorsCoverage.bas
' =================================================================================================
' TEST UNITARIO: CONSOLE — COSTANTI COLORE NOMINATE (ANSI-256)
' =================================================================================================
' COPERTURA:
' - 148 costanti colore nominate (palette ANSI-256), usate per costruire la
'   cartella `nome -> indice` e per il lookup per nome (`COLOR indice`).
'
' GENERATO da tools/gen_constants_coverage_samples.py — non modificare a mano.
' USO: COLOR ColorIndex("AQUAMARINE1") — oppure COLOR %AQUAMARINE1.
' =================================================================================================
IMPORT CONSOLE

DIM cn(300) AS STRING
DIM ci(300) AS INTEGER
DIM cnt AS INTEGER

' Indice di un colore nominato; 0 se il nome non esiste.
FUNCTION ColorIndex(nm AS STRING) AS INTEGER
    DIM i AS INTEGER
    FOR i = 1 TO cnt
        IF cn(i) = UPPER(nm) THEN
            ColorIndex = ci(i)
            EXIT FUNCTION
        END IF
    NEXT i
    ColorIndex = 0
END FUNCTION

SUB Main()
    DIM i AS INTEGER, j AS INTEGER, t AS INTEGER, s AS STRING
    cnt = 0

    ' --- tabella nome -> indice
    cnt = cnt + 1 : cn(cnt) = "BLACK" : ci(cnt) = %BLACK
    cnt = cnt + 1 : cn(cnt) = "MAROON" : ci(cnt) = %MAROON
    cnt = cnt + 1 : cn(cnt) = "GREEN" : ci(cnt) = %GREEN
    cnt = cnt + 1 : cn(cnt) = "OLIVE" : ci(cnt) = %OLIVE
    cnt = cnt + 1 : cn(cnt) = "NAVY" : ci(cnt) = %NAVY
    cnt = cnt + 1 : cn(cnt) = "PURPLE" : ci(cnt) = %PURPLE
    cnt = cnt + 1 : cn(cnt) = "TEAL" : ci(cnt) = %TEAL
    cnt = cnt + 1 : cn(cnt) = "SILVER" : ci(cnt) = %SILVER
    cnt = cnt + 1 : cn(cnt) = "GRAY" : ci(cnt) = %GRAY
    cnt = cnt + 1 : cn(cnt) = "RED" : ci(cnt) = %RED
    cnt = cnt + 1 : cn(cnt) = "LIME" : ci(cnt) = %LIME
    cnt = cnt + 1 : cn(cnt) = "YELLOW" : ci(cnt) = %YELLOW
    cnt = cnt + 1 : cn(cnt) = "BLUE" : ci(cnt) = %BLUE
    cnt = cnt + 1 : cn(cnt) = "FUCHSIA" : ci(cnt) = %FUCHSIA
    cnt = cnt + 1 : cn(cnt) = "AQUA" : ci(cnt) = %AQUA
    cnt = cnt + 1 : cn(cnt) = "WHITE" : ci(cnt) = %WHITE
    cnt = cnt + 1 : cn(cnt) = "GRAY0" : ci(cnt) = %GRAY0
    cnt = cnt + 1 : cn(cnt) = "NAVY_BLUE" : ci(cnt) = %NAVY_BLUE
    cnt = cnt + 1 : cn(cnt) = "DARK_BLUE" : ci(cnt) = %DARK_BLUE
    cnt = cnt + 1 : cn(cnt) = "BLUE3" : ci(cnt) = %BLUE3
    cnt = cnt + 1 : cn(cnt) = "BLUE3_2" : ci(cnt) = %BLUE3_2
    cnt = cnt + 1 : cn(cnt) = "BLUE1" : ci(cnt) = %BLUE1
    cnt = cnt + 1 : cn(cnt) = "DARK_GREEN" : ci(cnt) = %DARK_GREEN
    cnt = cnt + 1 : cn(cnt) = "DEEP_SKY_BLUE4" : ci(cnt) = %DEEP_SKY_BLUE4
    cnt = cnt + 1 : cn(cnt) = "DEEP_SKY_BLUE4_2" : ci(cnt) = %DEEP_SKY_BLUE4_2
    cnt = cnt + 1 : cn(cnt) = "DEEP_SKY_BLUE4_3" : ci(cnt) = %DEEP_SKY_BLUE4_3
    cnt = cnt + 1 : cn(cnt) = "DODGER_BLUE3" : ci(cnt) = %DODGER_BLUE3
    cnt = cnt + 1 : cn(cnt) = "DODGER_BLUE2" : ci(cnt) = %DODGER_BLUE2
    cnt = cnt + 1 : cn(cnt) = "GREEN4" : ci(cnt) = %GREEN4
    cnt = cnt + 1 : cn(cnt) = "SPRING_GREEN4" : ci(cnt) = %SPRING_GREEN4
    cnt = cnt + 1 : cn(cnt) = "TURQUOISE4" : ci(cnt) = %TURQUOISE4
    cnt = cnt + 1 : cn(cnt) = "DEEP_SKY_BLUE3" : ci(cnt) = %DEEP_SKY_BLUE3
    cnt = cnt + 1 : cn(cnt) = "DEEP_SKY_BLUE3_2" : ci(cnt) = %DEEP_SKY_BLUE3_2
    cnt = cnt + 1 : cn(cnt) = "DODGER_BLUE1" : ci(cnt) = %DODGER_BLUE1
    cnt = cnt + 1 : cn(cnt) = "GREEN3" : ci(cnt) = %GREEN3
    cnt = cnt + 1 : cn(cnt) = "SPRING_GREEN3" : ci(cnt) = %SPRING_GREEN3
    cnt = cnt + 1 : cn(cnt) = "DARK_CYAN" : ci(cnt) = %DARK_CYAN
    cnt = cnt + 1 : cn(cnt) = "LIGHT_SEA_GREEN" : ci(cnt) = %LIGHT_SEA_GREEN
    cnt = cnt + 1 : cn(cnt) = "DEEP_SKY_BLUE2" : ci(cnt) = %DEEP_SKY_BLUE2
    cnt = cnt + 1 : cn(cnt) = "DEEP_SKY_BLUE1" : ci(cnt) = %DEEP_SKY_BLUE1
    cnt = cnt + 1 : cn(cnt) = "GREEN3_2" : ci(cnt) = %GREEN3_2
    cnt = cnt + 1 : cn(cnt) = "SPRING_GREEN3_2" : ci(cnt) = %SPRING_GREEN3_2
    cnt = cnt + 1 : cn(cnt) = "SPRING_GREEN2" : ci(cnt) = %SPRING_GREEN2
    cnt = cnt + 1 : cn(cnt) = "CYAN3" : ci(cnt) = %CYAN3
    cnt = cnt + 1 : cn(cnt) = "DARK_TURQUOISE" : ci(cnt) = %DARK_TURQUOISE
    cnt = cnt + 1 : cn(cnt) = "TURQUOISE2" : ci(cnt) = %TURQUOISE2
    cnt = cnt + 1 : cn(cnt) = "GREEN1" : ci(cnt) = %GREEN1
    cnt = cnt + 1 : cn(cnt) = "SPRING_GREEN2_2" : ci(cnt) = %SPRING_GREEN2_2
    cnt = cnt + 1 : cn(cnt) = "SPRING_GREEN1" : ci(cnt) = %SPRING_GREEN1
    cnt = cnt + 1 : cn(cnt) = "MEDIUM_SPRING_GREEN" : ci(cnt) = %MEDIUM_SPRING_GREEN
    cnt = cnt + 1 : cn(cnt) = "CYAN2" : ci(cnt) = %CYAN2
    cnt = cnt + 1 : cn(cnt) = "CYAN1" : ci(cnt) = %CYAN1
    cnt = cnt + 1 : cn(cnt) = "DARK_RED" : ci(cnt) = %DARK_RED
    cnt = cnt + 1 : cn(cnt) = "DEEP_PINK4" : ci(cnt) = %DEEP_PINK4
    cnt = cnt + 1 : cn(cnt) = "PURPLE4" : ci(cnt) = %PURPLE4
    cnt = cnt + 1 : cn(cnt) = "PURPLE4_2" : ci(cnt) = %PURPLE4_2
    cnt = cnt + 1 : cn(cnt) = "PURPLE3" : ci(cnt) = %PURPLE3
    cnt = cnt + 1 : cn(cnt) = "BLUE_VIOLET" : ci(cnt) = %BLUE_VIOLET
    cnt = cnt + 1 : cn(cnt) = "ORANGE4" : ci(cnt) = %ORANGE4
    cnt = cnt + 1 : cn(cnt) = "GRAY37" : ci(cnt) = %GRAY37
    cnt = cnt + 1 : cn(cnt) = "MEDIUM_PURPLE4" : ci(cnt) = %MEDIUM_PURPLE4
    cnt = cnt + 1 : cn(cnt) = "SLATE_BLUE3" : ci(cnt) = %SLATE_BLUE3
    cnt = cnt + 1 : cn(cnt) = "SLATE_BLUE3_2" : ci(cnt) = %SLATE_BLUE3_2
    cnt = cnt + 1 : cn(cnt) = "ROYAL_BLUE1" : ci(cnt) = %ROYAL_BLUE1
    cnt = cnt + 1 : cn(cnt) = "CHARTREUSE4" : ci(cnt) = %CHARTREUSE4
    cnt = cnt + 1 : cn(cnt) = "DARK_SEA_GREEN4" : ci(cnt) = %DARK_SEA_GREEN4
    cnt = cnt + 1 : cn(cnt) = "PALE_TURQUOISE4" : ci(cnt) = %PALE_TURQUOISE4
    cnt = cnt + 1 : cn(cnt) = "STEEL_BLUE" : ci(cnt) = %STEEL_BLUE
    cnt = cnt + 1 : cn(cnt) = "STEEL_BLUE3" : ci(cnt) = %STEEL_BLUE3
    cnt = cnt + 1 : cn(cnt) = "CORNFLOWER_BLUE" : ci(cnt) = %CORNFLOWER_BLUE
    cnt = cnt + 1 : cn(cnt) = "CHARTREUSE3" : ci(cnt) = %CHARTREUSE3
    cnt = cnt + 1 : cn(cnt) = "DARK_SEA_GREEN4_2" : ci(cnt) = %DARK_SEA_GREEN4_2
    cnt = cnt + 1 : cn(cnt) = "CADET_BLUE" : ci(cnt) = %CADET_BLUE
    cnt = cnt + 1 : cn(cnt) = "CADET_BLUE_2" : ci(cnt) = %CADET_BLUE_2
    cnt = cnt + 1 : cn(cnt) = "SKY_BLUE3" : ci(cnt) = %SKY_BLUE3
    cnt = cnt + 1 : cn(cnt) = "STEEL_BLUE1" : ci(cnt) = %STEEL_BLUE1
    cnt = cnt + 1 : cn(cnt) = "CHARTREUSE3_2" : ci(cnt) = %CHARTREUSE3_2
    cnt = cnt + 1 : cn(cnt) = "PALE_GREEN3" : ci(cnt) = %PALE_GREEN3
    cnt = cnt + 1 : cn(cnt) = "SEA_GREEN3" : ci(cnt) = %SEA_GREEN3
    cnt = cnt + 1 : cn(cnt) = "AQUAMARINE3" : ci(cnt) = %AQUAMARINE3
    cnt = cnt + 1 : cn(cnt) = "MEDIUM_TURQUOISE" : ci(cnt) = %MEDIUM_TURQUOISE
    cnt = cnt + 1 : cn(cnt) = "STEEL_BLUE1_2" : ci(cnt) = %STEEL_BLUE1_2
    cnt = cnt + 1 : cn(cnt) = "CHARTREUSE2" : ci(cnt) = %CHARTREUSE2
    cnt = cnt + 1 : cn(cnt) = "SEA_GREEN2" : ci(cnt) = %SEA_GREEN2
    cnt = cnt + 1 : cn(cnt) = "SEA_GREEN1" : ci(cnt) = %SEA_GREEN1
    cnt = cnt + 1 : cn(cnt) = "SEA_GREEN1_2" : ci(cnt) = %SEA_GREEN1_2
    cnt = cnt + 1 : cn(cnt) = "AQUAMARINE1" : ci(cnt) = %AQUAMARINE1
    cnt = cnt + 1 : cn(cnt) = "DARK_SLATE_GRAY2" : ci(cnt) = %DARK_SLATE_GRAY2
    cnt = cnt + 1 : cn(cnt) = "DARK_RED_2" : ci(cnt) = %DARK_RED_2
    cnt = cnt + 1 : cn(cnt) = "DEEP_PINK4_2" : ci(cnt) = %DEEP_PINK4_2
    cnt = cnt + 1 : cn(cnt) = "DARK_MAGENTA" : ci(cnt) = %DARK_MAGENTA
    cnt = cnt + 1 : cn(cnt) = "DARK_MAGENTA_2" : ci(cnt) = %DARK_MAGENTA_2
    cnt = cnt + 1 : cn(cnt) = "DARK_VIOLET" : ci(cnt) = %DARK_VIOLET
    cnt = cnt + 1 : cn(cnt) = "PURPLE1" : ci(cnt) = %PURPLE1
    cnt = cnt + 1 : cn(cnt) = "ORANGE4_2" : ci(cnt) = %ORANGE4_2
    cnt = cnt + 1 : cn(cnt) = "LIGHT_PINK4" : ci(cnt) = %LIGHT_PINK4
    cnt = cnt + 1 : cn(cnt) = "PLUM4" : ci(cnt) = %PLUM4
    cnt = cnt + 1 : cn(cnt) = "MEDIUM_PURPLE3" : ci(cnt) = %MEDIUM_PURPLE3
    cnt = cnt + 1 : cn(cnt) = "MEDIUM_PURPLE3_2" : ci(cnt) = %MEDIUM_PURPLE3_2
    cnt = cnt + 1 : cn(cnt) = "SLATE_BLUE1" : ci(cnt) = %SLATE_BLUE1
    cnt = cnt + 1 : cn(cnt) = "YELLOW4" : ci(cnt) = %YELLOW4
    cnt = cnt + 1 : cn(cnt) = "WHEAT4" : ci(cnt) = %WHEAT4
    cnt = cnt + 1 : cn(cnt) = "GRAY53" : ci(cnt) = %GRAY53
    cnt = cnt + 1 : cn(cnt) = "LIGHT_SLATE_GRAY" : ci(cnt) = %LIGHT_SLATE_GRAY
    cnt = cnt + 1 : cn(cnt) = "MEDIUM_PURPLE" : ci(cnt) = %MEDIUM_PURPLE
    cnt = cnt + 1 : cn(cnt) = "LIGHT_SLATE_BLUE" : ci(cnt) = %LIGHT_SLATE_BLUE
    cnt = cnt + 1 : cn(cnt) = "YELLOW4_2" : ci(cnt) = %YELLOW4_2
    cnt = cnt + 1 : cn(cnt) = "DARK_OLIVE_GREEN3" : ci(cnt) = %DARK_OLIVE_GREEN3
    cnt = cnt + 1 : cn(cnt) = "DARK_SEA_GREEN" : ci(cnt) = %DARK_SEA_GREEN
    cnt = cnt + 1 : cn(cnt) = "LIGHT_SKY_BLUE3" : ci(cnt) = %LIGHT_SKY_BLUE3
    cnt = cnt + 1 : cn(cnt) = "LIGHT_SKY_BLUE3_2" : ci(cnt) = %LIGHT_SKY_BLUE3_2
    cnt = cnt + 1 : cn(cnt) = "SKY_BLUE2" : ci(cnt) = %SKY_BLUE2
    cnt = cnt + 1 : cn(cnt) = "CHARTREUSE2_2" : ci(cnt) = %CHARTREUSE2_2
    cnt = cnt + 1 : cn(cnt) = "DARK_OLIVE_GREEN3_2" : ci(cnt) = %DARK_OLIVE_GREEN3_2
    cnt = cnt + 1 : cn(cnt) = "PALE_GREEN3_2" : ci(cnt) = %PALE_GREEN3_2
    cnt = cnt + 1 : cn(cnt) = "DARK_SEA_GREEN3" : ci(cnt) = %DARK_SEA_GREEN3
    cnt = cnt + 1 : cn(cnt) = "DARK_SLATE_GRAY3" : ci(cnt) = %DARK_SLATE_GRAY3
    cnt = cnt + 1 : cn(cnt) = "SKY_BLUE1" : ci(cnt) = %SKY_BLUE1
    cnt = cnt + 1 : cn(cnt) = "CHARTREUSE1" : ci(cnt) = %CHARTREUSE1
    cnt = cnt + 1 : cn(cnt) = "LIGHT_GREEN" : ci(cnt) = %LIGHT_GREEN
    cnt = cnt + 1 : cn(cnt) = "LIGHT_GREEN_2" : ci(cnt) = %LIGHT_GREEN_2
    cnt = cnt + 1 : cn(cnt) = "PALE_GREEN1" : ci(cnt) = %PALE_GREEN1
    cnt = cnt + 1 : cn(cnt) = "AQUAMARINE1_2" : ci(cnt) = %AQUAMARINE1_2
    cnt = cnt + 1 : cn(cnt) = "DARK_SLATE_GRAY1" : ci(cnt) = %DARK_SLATE_GRAY1
    cnt = cnt + 1 : cn(cnt) = "GRAY2" : ci(cnt) = %GRAY2
    cnt = cnt + 1 : cn(cnt) = "GRAY3" : ci(cnt) = %GRAY3
    cnt = cnt + 1 : cn(cnt) = "GRAY7" : ci(cnt) = %GRAY7
    cnt = cnt + 1 : cn(cnt) = "GRAY11" : ci(cnt) = %GRAY11
    cnt = cnt + 1 : cn(cnt) = "GRAY15" : ci(cnt) = %GRAY15
    cnt = cnt + 1 : cn(cnt) = "GRAY19" : ci(cnt) = %GRAY19
    cnt = cnt + 1 : cn(cnt) = "GRAY23" : ci(cnt) = %GRAY23
    cnt = cnt + 1 : cn(cnt) = "GRAY27" : ci(cnt) = %GRAY27
    cnt = cnt + 1 : cn(cnt) = "GRAY30" : ci(cnt) = %GRAY30
    cnt = cnt + 1 : cn(cnt) = "GRAY35" : ci(cnt) = %GRAY35
    cnt = cnt + 1 : cn(cnt) = "GRAY39" : ci(cnt) = %GRAY39
    cnt = cnt + 1 : cn(cnt) = "GRAY42" : ci(cnt) = %GRAY42
    cnt = cnt + 1 : cn(cnt) = "GRAY46" : ci(cnt) = %GRAY46
    cnt = cnt + 1 : cn(cnt) = "GRAY50" : ci(cnt) = %GRAY50
    cnt = cnt + 1 : cn(cnt) = "GRAY54" : ci(cnt) = %GRAY54
    cnt = cnt + 1 : cn(cnt) = "GRAY58" : ci(cnt) = %GRAY58
    cnt = cnt + 1 : cn(cnt) = "GRAY62" : ci(cnt) = %GRAY62
    cnt = cnt + 1 : cn(cnt) = "GRAY66" : ci(cnt) = %GRAY66
    cnt = cnt + 1 : cn(cnt) = "GRAY70" : ci(cnt) = %GRAY70
    cnt = cnt + 1 : cn(cnt) = "GRAY74" : ci(cnt) = %GRAY74
    cnt = cnt + 1 : cn(cnt) = "GRAY78" : ci(cnt) = %GRAY78
    cnt = cnt + 1 : cn(cnt) = "GRAY82" : ci(cnt) = %GRAY82
    cnt = cnt + 1 : cn(cnt) = "GRAY85" : ci(cnt) = %GRAY85
    cnt = cnt + 1 : cn(cnt) = "GRAY89" : ci(cnt) = %GRAY89

    ' --- ordine per indice (l'ordine di registrazione segue la palette, non l'indice)
    FOR i = 1 TO cnt - 1
        FOR j = i + 1 TO cnt
            IF ci(j) < ci(i) THEN
                t = ci(i) : ci(i) = ci(j) : ci(j) = t
                s = cn(i) : cn(i) = cn(j) : cn(j) = s
            END IF
        NEXT j
    NEXT i

    PRINT "Costanti colore nominate (ANSI-256): "; cnt
    PRINT "--------------------------------------------------------------"
    FOR i = 1 TO cnt
        PRINT cn(i); TAB(30); ci(i)
    NEXT i
    PRINT "--------------------------------------------------------------"

    ' --- uso: lookup per nome (un colore nominato invece di un indice nudo)
    ' NB: in BASIC non esiste l'escape backslash dentro le stringhe: niente " qui.
    PRINT "ColorIndex AQUAMARINE1 = "; ColorIndex("AQUAMARINE1")
    PRINT "ColorIndex DEEP_SKY_BLUE4 = "; ColorIndex("DEEP_SKY_BLUE4")
    PRINT "ColorIndex INESISTENTE = "; ColorIndex("INESISTENTE")
    COLOR ColorIndex("AQUAMARINE1")
    PRINT "testo colorato con AQUAMARINE1"
    COLOR 7
    PRINT "Fine cartella colori."
END SUB
