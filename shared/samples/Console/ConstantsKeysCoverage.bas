' FILE: Console/ConstantsKeysCoverage.bas
' =================================================================================================
' TEST UNITARIO: CONSOLE — COSTANTI TASTO (%K_*)
' =================================================================================================
' COPERTURA:
' - 86 costanti tasto (%K_*): tasti funzione, navigazione, lettere, cifre,
'   numpad, speciali e modificatori; con lookup inverso codice -> nome.
'
' GENERATO da tools/gen_constants_coverage_samples.py — non modificare a mano.
' USO: IF tasto = %K_ESC THEN ... / PRINT KeyName(codice)
' =================================================================================================
IMPORT CONSOLE

DIM kn(200) AS STRING
DIM kc(200) AS INTEGER
DIM knum AS INTEGER

' Nome leggibile di un codice tasto; stringa vuota se sconosciuto.
FUNCTION KeyName(code AS INTEGER) AS STRING
    DIM i AS INTEGER
    FOR i = 1 TO knum
        IF kc(i) = code THEN
            KeyName = kn(i)
            EXIT FUNCTION
        END IF
    NEXT i
    KeyName = ""
END FUNCTION

SUB Main()
    DIM i AS INTEGER, j AS INTEGER, t AS INTEGER, s AS STRING
    knum = 0

    ' --- tabella codice -> nome
    knum = knum + 1 : kn(knum) = "K_F1" : kc(knum) = %K_F1
    knum = knum + 1 : kn(knum) = "K_F2" : kc(knum) = %K_F2
    knum = knum + 1 : kn(knum) = "K_F3" : kc(knum) = %K_F3
    knum = knum + 1 : kn(knum) = "K_F4" : kc(knum) = %K_F4
    knum = knum + 1 : kn(knum) = "K_F5" : kc(knum) = %K_F5
    knum = knum + 1 : kn(knum) = "K_F6" : kc(knum) = %K_F6
    knum = knum + 1 : kn(knum) = "K_F7" : kc(knum) = %K_F7
    knum = knum + 1 : kn(knum) = "K_F8" : kc(knum) = %K_F8
    knum = knum + 1 : kn(knum) = "K_F9" : kc(knum) = %K_F9
    knum = knum + 1 : kn(knum) = "K_F10" : kc(knum) = %K_F10
    knum = knum + 1 : kn(knum) = "K_F11" : kc(knum) = %K_F11
    knum = knum + 1 : kn(knum) = "K_F12" : kc(knum) = %K_F12
    knum = knum + 1 : kn(knum) = "K_UP" : kc(knum) = %K_UP
    knum = knum + 1 : kn(knum) = "K_DOWN" : kc(knum) = %K_DOWN
    knum = knum + 1 : kn(knum) = "K_LEFT" : kc(knum) = %K_LEFT
    knum = knum + 1 : kn(knum) = "K_RIGHT" : kc(knum) = %K_RIGHT
    knum = knum + 1 : kn(knum) = "K_HOME" : kc(knum) = %K_HOME
    knum = knum + 1 : kn(knum) = "K_END" : kc(knum) = %K_END
    knum = knum + 1 : kn(knum) = "K_PGUP" : kc(knum) = %K_PGUP
    knum = knum + 1 : kn(knum) = "K_PGDN" : kc(knum) = %K_PGDN
    knum = knum + 1 : kn(knum) = "K_INS" : kc(knum) = %K_INS
    knum = knum + 1 : kn(knum) = "K_DEL" : kc(knum) = %K_DEL
    knum = knum + 1 : kn(knum) = "K_ESC" : kc(knum) = %K_ESC
    knum = knum + 1 : kn(knum) = "K_TAB" : kc(knum) = %K_TAB
    knum = knum + 1 : kn(knum) = "K_ENTER" : kc(knum) = %K_ENTER
    knum = knum + 1 : kn(knum) = "K_BS" : kc(knum) = %K_BS
    knum = knum + 1 : kn(knum) = "K_SPC" : kc(knum) = %K_SPC
    knum = knum + 1 : kn(knum) = "K_PAUSE" : kc(knum) = %K_PAUSE
    knum = knum + 1 : kn(knum) = "K_PRTSC" : kc(knum) = %K_PRTSC
    knum = knum + 1 : kn(knum) = "K_A" : kc(knum) = %K_A
    knum = knum + 1 : kn(knum) = "K_B" : kc(knum) = %K_B
    knum = knum + 1 : kn(knum) = "K_C" : kc(knum) = %K_C
    knum = knum + 1 : kn(knum) = "K_D" : kc(knum) = %K_D
    knum = knum + 1 : kn(knum) = "K_E" : kc(knum) = %K_E
    knum = knum + 1 : kn(knum) = "K_F" : kc(knum) = %K_F
    knum = knum + 1 : kn(knum) = "K_G" : kc(knum) = %K_G
    knum = knum + 1 : kn(knum) = "K_H" : kc(knum) = %K_H
    knum = knum + 1 : kn(knum) = "K_I" : kc(knum) = %K_I
    knum = knum + 1 : kn(knum) = "K_J" : kc(knum) = %K_J
    knum = knum + 1 : kn(knum) = "K_K" : kc(knum) = %K_K
    knum = knum + 1 : kn(knum) = "K_L" : kc(knum) = %K_L
    knum = knum + 1 : kn(knum) = "K_M" : kc(knum) = %K_M
    knum = knum + 1 : kn(knum) = "K_N" : kc(knum) = %K_N
    knum = knum + 1 : kn(knum) = "K_O" : kc(knum) = %K_O
    knum = knum + 1 : kn(knum) = "K_P" : kc(knum) = %K_P
    knum = knum + 1 : kn(knum) = "K_Q" : kc(knum) = %K_Q
    knum = knum + 1 : kn(knum) = "K_R" : kc(knum) = %K_R
    knum = knum + 1 : kn(knum) = "K_S" : kc(knum) = %K_S
    knum = knum + 1 : kn(knum) = "K_T" : kc(knum) = %K_T
    knum = knum + 1 : kn(knum) = "K_U" : kc(knum) = %K_U
    knum = knum + 1 : kn(knum) = "K_V" : kc(knum) = %K_V
    knum = knum + 1 : kn(knum) = "K_W" : kc(knum) = %K_W
    knum = knum + 1 : kn(knum) = "K_X" : kc(knum) = %K_X
    knum = knum + 1 : kn(knum) = "K_Y" : kc(knum) = %K_Y
    knum = knum + 1 : kn(knum) = "K_Z" : kc(knum) = %K_Z
    knum = knum + 1 : kn(knum) = "K_0" : kc(knum) = %K_0
    knum = knum + 1 : kn(knum) = "K_1" : kc(knum) = %K_1
    knum = knum + 1 : kn(knum) = "K_2" : kc(knum) = %K_2
    knum = knum + 1 : kn(knum) = "K_3" : kc(knum) = %K_3
    knum = knum + 1 : kn(knum) = "K_4" : kc(knum) = %K_4
    knum = knum + 1 : kn(knum) = "K_5" : kc(knum) = %K_5
    knum = knum + 1 : kn(knum) = "K_6" : kc(knum) = %K_6
    knum = knum + 1 : kn(knum) = "K_7" : kc(knum) = %K_7
    knum = knum + 1 : kn(knum) = "K_8" : kc(knum) = %K_8
    knum = knum + 1 : kn(knum) = "K_9" : kc(knum) = %K_9
    knum = knum + 1 : kn(knum) = "K_N0" : kc(knum) = %K_N0
    knum = knum + 1 : kn(knum) = "K_N1" : kc(knum) = %K_N1
    knum = knum + 1 : kn(knum) = "K_N2" : kc(knum) = %K_N2
    knum = knum + 1 : kn(knum) = "K_N3" : kc(knum) = %K_N3
    knum = knum + 1 : kn(knum) = "K_N4" : kc(knum) = %K_N4
    knum = knum + 1 : kn(knum) = "K_N5" : kc(knum) = %K_N5
    knum = knum + 1 : kn(knum) = "K_N6" : kc(knum) = %K_N6
    knum = knum + 1 : kn(knum) = "K_N7" : kc(knum) = %K_N7
    knum = knum + 1 : kn(knum) = "K_N8" : kc(knum) = %K_N8
    knum = knum + 1 : kn(knum) = "K_N9" : kc(knum) = %K_N9
    knum = knum + 1 : kn(knum) = "K_NDIV" : kc(knum) = %K_NDIV
    knum = knum + 1 : kn(knum) = "K_NMUL" : kc(knum) = %K_NMUL
    knum = knum + 1 : kn(knum) = "K_NSUB" : kc(knum) = %K_NSUB
    knum = knum + 1 : kn(knum) = "K_NADD" : kc(knum) = %K_NADD
    knum = knum + 1 : kn(knum) = "K_NDEC" : kc(knum) = %K_NDEC
    knum = knum + 1 : kn(knum) = "K_CTRL_C" : kc(knum) = %K_CTRL_C
    knum = knum + 1 : kn(knum) = "K_CTRL_BRK" : kc(knum) = %K_CTRL_BRK
    knum = knum + 1 : kn(knum) = "K_SHIFT_TAB" : kc(knum) = %K_SHIFT_TAB
    knum = knum + 1 : kn(knum) = "K_ALT" : kc(knum) = %K_ALT
    knum = knum + 1 : kn(knum) = "K_SHIFT" : kc(knum) = %K_SHIFT
    knum = knum + 1 : kn(knum) = "K_CTRL" : kc(knum) = %K_CTRL

    ' --- ordine per codice (i codici ConsoleKey non sono contigui)
    FOR i = 1 TO knum - 1
        FOR j = i + 1 TO knum
            IF kc(j) < kc(i) THEN
                t = kc(i) : kc(i) = kc(j) : kc(j) = t
                s = kn(i) : kn(i) = kn(j) : kn(j) = s
            END IF
        NEXT j
    NEXT i

    PRINT "Costanti tasto (%K_*): "; knum
    PRINT "--------------------------------------------------------------"
    FOR i = 1 TO knum
        PRINT kn(i); TAB(16); kc(i)
    NEXT i
    PRINT "--------------------------------------------------------------"

    ' --- uso: dispatch non bloccante su un codice tasto, come in un ciclo di eventi
    DIM tasto AS INTEGER
    tasto = %K_ESC
    IF tasto = %K_ESC THEN PRINT "ESC -> "; KeyName(tasto)
    tasto = %K_F1
    IF tasto = %K_F1 THEN PRINT "F1  -> "; KeyName(tasto)
    tasto = %K_UP
    IF tasto = %K_UP THEN PRINT "UP  -> "; KeyName(tasto)
    PRINT "KeyName(9999) = ["; KeyName(9999); "]"
    PRINT "Fine mappa tasti."
END SUB
