' FILE: Controls/ControlPropertiesCoverage.bas
' =================================================================================================
' TEST UNITARIO: CONTROLS — ACCESSOR PROPRIETÀ (CTRLGETPROP / CTRLGETPROPS)
' =================================================================================================
' Lettura di una proprietà di un controllo per NOME: sono le funzioni su cui si appoggia lo
' zucchero `istanza.Prop`. Ritorni diversi per tipo:
'   CTRLGETPROP(nome, prop)  -> DOUBLE   proprietà numerica (Left, Top, Width, Height, ...)
'   CTRLGETPROPS(nome, prop) -> STRING   proprietà testuale (Caption, Text, ...)
'
' Richiedono un'istanza di controllo realmente creata (modalità GUI, `SCREEN n`): in headless
' la creazione non avviene, quindi le chiamate stanno in `IF 0 THEN` — stessa convenzione di
' minitests/misc/surface_missing_symbols.bas — e verificano firma e arità.
'
' Guida: Guide/utente/Controlli-Utente-Base-Control.md (sezione "Accesso alle proprietà").
' =================================================================================================
IMPORT CONTROLS

SUB Main()
    IF 0 THEN
        DIM posizione AS DOUBLE
        DIM didascalia AS STRING
        posizione = CTRLGETPROP("btn1", "Left")
        didascalia = CTRLGETPROPS("btn1", "Caption")
    END IF

    PRINT "CONTROLS — accessor proprietà"
    PRINT "--------------------------------------------------"
    PRINT "CTRLGETPROP(nome, prop)  -> DOUBLE  (es. Left)"
    PRINT "CTRLGETPROPS(nome, prop) -> STRING  (es. Caption)"
    PRINT "Sintassi verificata; l'esecuzione reale richiede un controllo creato in modalità GUI"
    PRINT "(SCREEN n): in headless il controllo non esiste."
    PRINT "--------------------------------------------------"
    PRINT "Fine."
END SUB
