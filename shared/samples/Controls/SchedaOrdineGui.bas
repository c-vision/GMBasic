' FILE: Controls/SchedaOrdineGui.bas
' SCENARIO DIDATTICO — scheda di raccolta ordine con i controlli GUI.
'
' Un cameriere prende un ordine da asporto. La scheda mostra, nell'ordine in cui
' compaiono sullo schermo:
'   LABEL          → testo descrittivo (display-only)
'   NUMERICUPDOWN  → porzioni, con minimo/massimo/passo (l'interfaccia non permette
'                    valori fuori scala: la validazione la fa il controllo)
'   COMBOBOX       → formato (fetta / torta intera / vassoio)
'   CHECKBOX       → confezione regalo
'   ALERT          → riepilogo modale prima di chiudere la scheda
'
' Ogni controllo è MODALE: nel programma reale l'utente edita e conferma (INVIO) o
' annulla (ESC); il valore confermato torna nella variabile indicata con INTO.
' In CI il programma inietta SENDKEY CHR(27) per chiudere ciascun controllo, come gli
' altri sample di Controls/.
'
' Domini: CONTROLS + MATH. Output deterministico (i valori mostrati sono quelli noti
' al programma: la scheda dimostra il flusso dei controlli, non l'input umano).
IMPORT CONTROLS
IMPORT MATH

SUB Main()
    ' Valori dell'ordine (in un'applicazione arriverebbero dai controlli).
    DIM porzioni AS INTEGER
    DIM regalo AS INTEGER
    DIM formato AS STRING
    porzioni = 6
    regalo = 0
    formato = "Fetta"

    ' ── 1. Etichetta descrittiva ─────────────────────────────────────────────
    PRINT "== 1. Intestazione =="
    PRINT "cliente: Mario Bianchi"
    LABEL 4, 4, 40, "Cliente: Mario Bianchi"
    SENDKEY CHR(27)

    ' ── 2. Campo numerico: porzioni ──────────────────────────────────────────
    ' min 1, max 20, passo 1: il controllo accetta solo valori nell'intervallo.
    PRINT "== 2. Porzioni =="
    ' La variabile INTO riceve il valore CONFERMATO dall'utente. In CI il controllo
    ' viene chiuso con ESC (annulla), quindi si usa una variabile di servizio: il
    ' valore dell'ordine resta quello impostato dal programma.
    DIM campoPorzioni AS INTEGER
    campoPorzioni = porzioni
    NUMERICUPDOWN 6, 4, 10, 1, 20, 1 INTO campoPorzioni
    SENDKEY CHR(27)
    PRINT "porzioni: "; porzioni

    ' ── 3. Scelta da elenco: formato ─────────────────────────────────────────
    PRINT "== 3. Formato =="
    DIM campoFormato AS STRING
    campoFormato = formato
    COMBOBOX 8, 4, 18, {"Fetta", "Torta intera", "Vassoio"} INTO campoFormato
    SENDKEY CHR(27)
    PRINT "formato: "; formato

    ' ── 4. Casella di spunta: confezione regalo ──────────────────────────────
    PRINT "== 4. Regalo =="
    DIM campoRegalo AS INTEGER
    campoRegalo = regalo
    CHECKBOX 10, 4, 30, "Confezione regalo" INTO campoRegalo
    SENDKEY CHR(27)
    PRINT "regalo: "; regalo

    ' ── 5. Riepilogo con ALERT modale ────────────────────────────────────────
    ' ALERT è un dialogo: serve SENDKEY anche per lui. Il totale si calcola PRIMA,
    ' così il messaggio mostra il valore definitivo.
    DIM totale AS DOUBLE
    totale = porzioni * 4.5
    IF regalo <> 0 THEN totale = totale + 2.0
    PRINT "== 5. Riepilogo =="
    ALERT("Ordine: " + TRIM$(STR$(porzioni)) + " x " + formato + " = " + TRIM$(STR$(MATH.ROUND(totale, 2))) + " EUR")
    SENDKEY CHR(27)
    PRINT "riga d'ordine composta"

    ' ── 6. Confezione del risultato ──────────────────────────────────────────
    ' La scheda chiude restituendo una riga pronta per il banco.
    PRINT "== 6. Scheda chiusa =="
    PRINT "riepilogo: "; porzioni; " x "; formato; " = "; MATH.ROUND(totale, 2); " EUR"
    PRINT "(regalo: "; regalo; ")"
END SUB
