' ============================================================================
' EventModelDemo.bas — modello eventi controlli/finestre (GMBasic).
' Usa TUTTA la superficie implementata:
'   - ID <var> (handle del controllo) su PIU' controlli
'   - clausole evento inline ATTACCATE: ONCLICK / ONCHANGE / ONGOTFOCUS
'   - forme azione: CALL sub() / sub / GOSUB <label> / GOTO <label> / STOP
'   - eventi RAGGRUPPATI: ON <EVENTO> CONTROL <id> <azione> (sovrascrivono l'inline)
'   - POLLING di stato: CLICKED/DOUBLECLICKED/CHANGED/GOTFOCUSED/LOSTFOCUSED(id),
'     MOUSECLICKED/MOUSEDBLCLICKED/MOUSEPRESSED/KEYPRESSED()
'   - eventi FINESTRA per handle: ON WINDOW <EV> OF <handle|nome> <azione>
' ============================================================================

IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 0

DIM chk   = 0        ' valore CHECK
DIM nome  = ""       ' valore TEXTBOX
DIM hChk  = -1       ' handle CHECK
DIM hBtn  = -1       ' handle BUTTON
DIM hTxt  = -1       ' handle TEXTBOX
DIM hBtn2 = -1       ' handle secondo BUTTON

' Etichette di modulo usate come handler di evento (GOSUB/GOTO): promosse a subroutine.
' ---------------------------------------------------------------------------
SUB Main()
    ' --- creazione controlli con ID + eventi inline ATTACCATI ---
    CHECKBOX 40, 60, "Attiva" INTO chk ID hChk NAME "chkAttiva" ONCLICK OnClick ONCHANGE OnChange
    BUTTON 40, 110, 160, 36, "Conferma" ID hBtn NAME "btnOk" ONCLICK OnClick
    TEXTBOX 40, 170, 240, 28 INTO nome ID hTxt NAME "txtNome" ONGOTFOCUS OnFocusSub
    BUTTON 240, 110, 160, 36, "Annulla" ID hBtn2 NAME "btnNo" ONCLICK STOP   ' nessun gestore

    ' --- eventi RAGGRUPPATI (per handle): sovrascrivono l'inline e usano le 4 forme ---
    ON CLICK  CONTROL hBtn  CALL OnClick        ' CALL esplicita
    ON CHANGE CONTROL hChk  GOSUB OnSoloChange  ' GOSUB verso label di modulo
    ON CLICK  CONTROL hBtn2 GOTO  OnSoloChange  ' GOTO verso label di modulo

    ' --- eventi FINESTRA per handle (0 = finestra SDL principale) ---
    ON WINDOW CLOSE    OF 0 GOSUB OnWinClose
    ON WINDOW MINIMIZE OF 0 CALL  OnWinMin()

    ' --- POLLING di stato (non consuma) ---
    PRINT "click=";     CLICKED(hChk)
    PRINT "dblclick=";  DOUBLECLICKED(hChk)
    PRINT "changed=";   CHANGED(hChk)
    PRINT "gotfocus=";  GOTFOCUSED(hChk)
    PRINT "lostfocus="; LOSTFOCUSED(hChk)
    PRINT "mouse=";     MOUSECLICKED()
    PRINT "mousedbl=";  MOUSEDBLCLICKED()
    PRINT "pressed=";   MOUSEPRESSED()
    PRINT "key=";       KEYPRESSED()

    PRINT "event-model-demo-ok"
END SUB

SUB OnClick()
    chk = 1
END SUB

SUB OnChange()
    chk = 0
END SUB

SUB OnSoloChange()
    chk = 1
END SUB

SUB OnWinClose()
    PRINT "window-close"
END SUB

SUB OnWinMin()
    PRINT "window-minimize"
END SUB

SUB OnFocusSub()
    nome = nome
END SUB
