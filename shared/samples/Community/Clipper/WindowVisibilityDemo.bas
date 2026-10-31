' WindowVisibilityDemo.bas
' ---------------------------------------------------------------------------
' Sample completo: WINDOW SHOW / WINDOW HIDE (forma globale: visibilita' della
' finestra dello screen) assieme ai controlli pixel e alla famiglia
' WINDOW name ACTION (DEFINE/ACTIVATE/PRINT/CLEAR/CLOSE).
'
' Comportamento:
'   - la finestra parte VISIBILE (SCREEN + WINDOW SHOW);
'   - "Nascondi"  -> WINDOW HIDE  (nasconde la finestra; il programma resta vivo);
'   - "Mostra"    -> WINDOW SHOW  (la rimostra);
'   - "Log"       -> WINDOW DEFINE/ACTIVATE/PRINT/CLEAR sulla finestra nominata L;
'   - "Esci"      -> chiude il form (FORM_CLOSE);
'   - "Chiudi log"-> WINDOW L CLOSE (rimuove la finestra nominata).
'
' Parita': il comando e' riconosciuto/emesso su tutti e 5 i backend (IL/PCode/VM/
' C99/C99Native); in headless SHOW/HIDE sono no-op osservabili.
' ---------------------------------------------------------------------------
IMPORT WINDOWS
IMPORT CONTROLS

SCREEN 12, 760, 440, 32
DIM gSel = 0
DIM gHidden = 0

SUB Main()
    WINDOW THEME "light"
    WINDOW SHOW

    LISTBOX 20, 40, 420, 320, "alpha|beta|gamma|delta|epsilon" INTO gSel
    STATUSBAR 20, 380, 720, "WINDOW SHOW / WINDOW HIDE - usa i pulsanti"

    BUTTON 470, 40, 180, 34, "Nascondi (WINDOW HIDE)" ON CLICK DoHide STYLE "rounded radius=7 bg=#b91c1c fg=white"
    BUTTON 470, 82, 180, 34, "Mostra (WINDOW SHOW)" ON CLICK DoShow STYLE "rounded radius=7 bg=#15803d fg=white"
    BUTTON 470, 124, 180, 34, "Log finestra" ON CLICK DoLog STYLE "rounded radius=7 bg=#1e3a8a fg=#e0f2fe"
    BUTTON 470, 166, 180, 34, "Chiudi log" ON CLICK DoCloseLog STYLE "rounded radius=7 bg=#334155 fg=#cbd5e1"
    BUTTON 470, 208, 180, 34, "Esci" ON CLICK DoExit STYLE "rounded radius=7 bg=#0b1220 fg=#cbd5e1 border=1,#94a3b8"

    GO SUB EventLoop()
END SUB

SUB EventLoop()
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB

SUB DoHide()
    WINDOW HIDE
    gHidden = 1
END SUB

SUB DoShow()
    WINDOW SHOW
    gHidden = 0
END SUB

SUB DoLog()
    WINDOW DEFINE L FROM 4, 44 TO 18, 110 TITLE "Log"
    WINDOW L ACTIVATE
    WINDOW L CLEAR
    WINDOW L PRINT "SHOW/HIDE parity"
    WINDOW L PRINT "gHidden ="
END SUB

SUB DoCloseLog()
    WINDOW L CLOSE
END SUB

SUB DoExit()
    WINDOW CLOSE
END SUB
