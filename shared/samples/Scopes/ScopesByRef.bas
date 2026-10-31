' FILE: Scopes/ScopesByRef.bas
' ======================================================================================================================
' GUIDA TEORICA: PASSAGGIO PARAMETRI BYREF E CREAZIONE IMPLICITA
' ======================================================================================================================
' GM-BASIC supporta il passaggio dei parametri per riferimento (BYREF), che permette alla procedura chiamata
' di modificare direttamente la variabile del chiamante.
'
' 1. SEMANTICA BYREF
'    - Quando un parametro è dichiarato `BYREF`, non viene passata una copia del valore, ma un riferimento
'      alla locazione di memoria della variabile originale.
'    - Qualsiasi assegnazione al parametro all'interno della SUB/FUNCTION si riflette immediatamente
'      sulla variabile passata.
'
' 2. CREAZIONE IMPLICITA AL MOMENTO DELLA CHIAMATA ("Auto-vivification")
'    - Una caratteristica potente (e pericolosa) del BASIC classico supportata da GM-BASIC è la creazione
'      automatica di variabili passate a una funzione se queste non esistono.
'    - Se si chiama `MioSub(NuovaVar)` e `NuovaVar` non è mai stata dichiarata o usata prima:
'      1. Il compilatore (fase Rewriter) rileva che `NuovaVar` non esiste.
'      2. Rileva che il parametro corrispondente di `MioSub` è `BYREF`.
'      3. Invece di lanciare errore "Variabile non definita", crea implicitamente `NuovaVar` nello scope locale
'         (con valore di default 0 o "") e passa il suo riferimento.
'      4. Dopo la chiamata, `NuovaVar` esiste e contiene il valore eventualmente modificato dalla SUB.
' ======================================================================================================================

SUB TestByRef(BYREF V)
  PRINT "  [SUB] Dentro TestByRef. V ricevuto. Assegno 10."
  V = 10 
END SUB

SUB TestByRefString(BYREF S$)
  PRINT "  [SUB] Dentro TestByRefString. S$ ricevuto. Assegno 'CREATO'."
  S$ = "CREATO"
END SUB

1 CLS
2 PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ ScopesByRef - Passaggio BYREF e Creazione Implicita                     ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
5 PRINT

20 PRINT "--- 1. Test Numerico con Inizializzazione Esplicita (LET) ---"
30 PRINT "Dichiaro esplicitamente A = 0..."
35 LET A = 0
40 PRINT "Chiamo TestByRef(A)..."
45 TestByRef(A)
50 PRINT "Valore di A dopo la chiamata:"; A; "(Atteso: 10)"
60 IF A = 10 THEN PRINT "  -> ESITO: SUCCESSO" ELSE PRINT "  -> ESITO: FALLITO"
70 PRINT

80 PRINT "--- 2. Test Stringa con Inizializzazione Esplicita (LET) ---"
90 PRINT "Dichiaro esplicitamente B$ = ''..."
95 LET B$ = ""
100 PRINT "Chiamo TestByRefString(B$)..."
105 TestByRefString(B$)
110 PRINT "Valore di B$ dopo la chiamata: '"; B$; "' (Atteso: CREATO)"
120 IF B$ = "CREATO" THEN PRINT "  -> ESITO: SUCCESSO" ELSE PRINT "  -> ESITO: FALLITO"
130 PRINT

' ' VERIFICA DI COMPLETEZZA
' Integrazione: Test della vera creazione implicita (senza LET precedente)
' Questo esercita la logica specifica del Rewriter (RewriteUnresolvedCall).

140 PRINT "--- 3. Test Numerico con Creazione IMPLICITA (Nessun LET) ---"
150 PRINT "Chiamo TestByRef(C) senza aver MAI usato C prima..."
' C non esiste. Il compilatore deve crearlo al volo.
160 TestByRef(C) 
170 PRINT "Valore di C dopo la chiamata:"; C; "(Atteso: 10)"
180 IF C = 10 THEN PRINT "  -> ESITO: SUCCESSO" ELSE PRINT "  -> ESITO: FALLITO"
190 PRINT

200 PRINT "--- 4. Test Stringa con Creazione IMPLICITA (Nessun LET) ---"
210 PRINT "Chiamo TestByRefString(D$) senza aver MAI usato D$ prima..."
' D$ non esiste.
220 TestByRefString(D$)
230 PRINT "Valore di D$ dopo la chiamata: '"; D$; "' (Atteso: CREATO)"
240 IF D$ = "CREATO" THEN PRINT "  -> ESITO: SUCCESSO" ELSE PRINT "  -> ESITO: FALLITO"
250 PRINT

260 PRINT "--- Test completato. ---"
270 END