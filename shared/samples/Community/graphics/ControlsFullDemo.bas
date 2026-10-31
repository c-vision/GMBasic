' ControlsFullDemo.bas — Sample COMPLETO: controlli pixel + proprietà base (VB)
' + eventi + metodi a runtime CONTROL (CONTROL SET/GET/GETS/MOVE/FOCUS/ZORDER).
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 12, 900, 620, 32

DIM attivo = 1
DIM livello = 1
DIM volume = 35
DIM lingua = 0
DIM avanzamento = 20
DIM interruttore = 0
DIM quantita = 3
DIM scheda = 0
DIM scelta = -1
DIM colore = 0
DIM riga = -1
DIM nodo = -1
DIM nome AS STRING
DIM menu0 = 0
DIM lettura = 0
DIM etichetta AS STRING

SUB Main()
    WINDOW THEME "Monokai"

    BUTTON 40, 30, 150, 40, "Ricomincia" ON CLICK OnRicomincia NAME "btnReset" TOOLTIP "Azzera" TAG "barra" TABINDEX 0 STYLE "rounded radius=10 emboss"
    MENU 40, 80, "File|Modifica|Aiuto" INTO menu0 NAME "mnuPrincipale"
    CHECKBOX 40, 120, "Attiva" INTO attivo NAME "chkAttiva" TOOLTIP "on/off" TAG "grp1" TABINDEX 1 ON CHANGE OnAttiva
    RADIO 40, 160, "Basso|Medio|Alto" INTO livello NAME "radLivello" TABINDEX 2 ON CHANGE OnLivello
    SLIDER 40, 200, 260, 0, 100, 5 INTO volume NAME "sldVolume" TAG "grp1" TABINDEX 3 ON SCROLL OnVolume ON CHANGE OnVolume
    COMBOBOX 40, 240, 260, "Italiano|English|Deutsch" INTO lingua NAME "cmbLingua" TABINDEX 4 ON DROPDOWN OnApri ON CHANGE OnLingua
    PROGRESS 40, 280, 260 INTO avanzamento
    TOGGLE 40, 310, "Esperto" INTO interruttore NAME "tglEsperto" TABINDEX 5 ON CHANGE OnEsperto
    SPINNER 40, 350, 150, 0, 20, 1 INTO quantita NAME "spnQta" TABINDEX 6 ON UPCLICK OnQta ON DOWNCLICK OnQta
    TAB 40, 390, "Generale|Dettagli" INTO scheda NAME "tabSezioni" ON CHANGE OnScheda
    LISTBOX 340, 30, 240, 120, "Uno|Due|Tre|Quattro" INTO scelta NAME "lstVoci" TABINDEX 7 ON SELCHANGE OnVoce
    COLORPICKER 340, 180 INTO colore NAME "cpColore" ON CHANGE OnColore
    GRID 340, 260, 500, 150, "Nome|Valore", "alfa|1;beta|2;gamma|3" INTO riga NAME "grdDati" ON ROWCOLCHANGE OnRiga ON SELCHANGE OnRiga
    TREEVIEW 340, 430, 240, 150, "radice|radice/f1|radice/f2|altra" INTO nodo NAME "treAlbero" ON NODECLICK OnNodo ON EXPAND OnEspandi ON COLLAPSE OnComprimi
    STATUSBAR 40, 590, 800, "pronto"
    TEXTBOX 600, 430, 240, 30 INTO nome NAME "txtNome" TOOLTIP "Scrivi un nome" TABINDEX 8 ON CHANGE OnNome ON VALIDATE OnNome ON KEYDOWN OnTasto ON GOTFOCUS OnFocusSu ON LOSTFOCUS OnFocusGiu ON DRAGOVER OnDragSu ON DRAGDROP OnRilascia
    BUTTON 600, 480, 240, 40, "MOSTRA" ON CLICK OnMostra NAME "btnMostra" ON DBLCLICK OnDoppio

    GO SUB EventLoop()
END SUB

SUB EventLoop()
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB

SUB OnRicomincia()
    CONTROL SET "chkAttiva", "value", 1
    CONTROL SET "sldVolume", "value", 35
    CONTROL SET "cmbLingua", "value", 0
    CONTROL MOVE "btnMostra", 600, 480, 240, 40
    CONTROL SET "btnMostra", "visible", 1
    CONTROL FOCUS "txtNome"
    CONTROL ZORDER "btnReset", 1
    CONTROL GET "btnReset", "left" INTO lettura
    CONTROL GETS "btnReset", "caption" INTO etichetta
    PRINT etichetta; " riparte a x="; lettura
END SUB

SUB OnAttiva()
    PRINT "Attiva = "; attivo
END SUB
SUB OnLivello()
    PRINT "Livello = "; livello
END SUB
SUB OnVolume()
    PRINT "Volume = "; volume
END SUB
SUB OnApri()
    PRINT "Menu aperto"
END SUB
SUB OnLingua()
    PRINT "Lingua = "; lingua
END SUB
SUB OnEsperto()
    PRINT "Esperto = "; interruttore
END SUB
SUB OnQta()
    PRINT "Quantita = "; quantita
END SUB
SUB OnScheda()
    PRINT "Scheda = "; scheda
END SUB
SUB OnVoce()
    PRINT "Voce = "; scelta
END SUB
SUB OnColore()
    PRINT "Colore = "; colore
END SUB
SUB OnRiga()
    PRINT "Riga = "; riga
END SUB
SUB OnNodo()
    PRINT "Nodo = "; nodo
END SUB
SUB OnEspandi()
    PRINT "Espanso"
END SUB
SUB OnComprimi()
    PRINT "Compresso"
END SUB
SUB OnNome()
    PRINT "Nome = "; nome
END SUB
SUB OnTasto()
END SUB
SUB OnFocusSu()
    PRINT "focus su"
END SUB
SUB OnFocusGiu()
    PRINT "focus perso"
END SUB
SUB OnDragSu()
END SUB
SUB OnRilascia()
    PRINT "rilasciato"
END SUB
SUB OnDoppio()
    PRINT "doppio click"
END SUB

SUB OnMostra()
    CONTROL GET "btnMostra", "visible" INTO lettura
    IF lettura = 1 THEN
        CONTROL SET "btnMostra", "visible", 0
        PRINT "nascosto"
    ELSE
        CONTROL SET "btnMostra", "visible", 1
        PRINT "mostrato"
    END IF
END SUB
