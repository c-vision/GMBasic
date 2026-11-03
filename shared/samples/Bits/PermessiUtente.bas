' FILE: Bits/PermessiUtente.bas
' SCENARIO DIDATTICO — permessi di accesso tenuti in un intero a bit.
'
' Un gestionale deve sapere CHI può fare COSA. Invece di una tabella con una riga per
' ogni permesso, si usa un solo intero come maschera di bit: il bit n vale 1 (in BASIC
' -1) se il permesso n è concesso. Costruire i ruoli diventa un'operazione sui bit e il
' controllo di un permesso costa un AND invece di una ricerca.
'
' Domini: BITS + base. Output deterministico, nessun I/O.
IMPORT BITS

' Bit del nostro sistema (0-based).
'   0 = LETTURA   1 = SCRITTURA  2 = ESECUZIONE  3 = CANCELLAZIONE
'   4 = ESPORTAZIONE  5 = AMMINISTRAZIONE
FUNCTION Concedi(mask AS LONG, bit AS LONG) AS LONG
    RETURN BITS.BITSET(mask, bit)
END FUNCTION

FUNCTION Revoca(mask AS LONG, bit AS LONG) AS LONG
    RETURN BITS.BITCLEAR(mask, bit)
END FUNCTION

FUNCTION HaPermesso(mask AS LONG, bit AS LONG) AS LONG
    RETURN BITS.BITTEST(mask, bit)
END FUNCTION

' Nome leggibile del permesso: serve per stampare i ruoli.
FUNCTION NomePermesso(bit AS LONG) AS STRING
    IF bit = 0 THEN RETURN "LETTURA"
    IF bit = 1 THEN RETURN "SCRITTURA"
    IF bit = 2 THEN RETURN "ESECUZIONE"
    IF bit = 3 THEN RETURN "CANCELLAZIONE"
    IF bit = 4 THEN RETURN "ESPORTAZIONE"
    RETURN "AMMINISTRAZIONE"
END FUNCTION

SUB Main()
    ' ── 1. Costruzione dei ruoli con i singoli bit ───────────────────────────
    ' Ospite: solo lettura.
    DIM ospite AS LONG
    ospite = 0
    ospite = Concedi(ospite, 0)

    ' Impiegato: legge, scrive ed esporta.
    DIM impiegato AS LONG
    impiegato = 0
    impiegato = Concedi(impiegato, 0)
    impiegato = Concedi(impiegato, 1)
    impiegato = Concedi(impiegato, 4)

    ' Amministratore: tutti i permessi.
    DIM admin AS LONG
    admin = 0
    DIM b AS LONG
    FOR b = 0 TO 5
        admin = Concedi(admin, b)
    NEXT b

    PRINT "== 1. Maschere di partenza =="
    PRINT "ospite:    "; ospite; "    permessi attivi: "; BITS.BITCOUNT(ospite)
    PRINT "impiegato: "; impiegato; "   permessi attivi: "; BITS.BITCOUNT(impiegato)
    PRINT "admin:     "; admin; "  permessi attivi: "; BITS.BITCOUNT(admin)

    ' ── 2. Quali permessi ha ciascun ruolo? ──────────────────────────────────
    PRINT "== 2. Permessi per ruolo =="
    DIM ruolo AS LONG
    FOR ruolo = 0 TO 2
        DIM mask AS LONG
        DIM chi AS STRING
        IF ruolo = 0 THEN
            mask = ospite
            chi = "ospite"
        ELSEIF ruolo = 1 THEN
            mask = impiegato
            chi = "impiegato"
        ELSE
            mask = admin
            chi = "admin"
        END IF
        PRINT chi; ":";
        FOR b = 0 TO 5
            IF HaPermesso(mask, b) THEN PRINT " "; NomePermesso(b);
        NEXT b
        PRINT ""
    NEXT ruolo

    ' ── 3. Modifica di un ruolo ──────────────────────────────────────────────
    ' L'impiegato non deve più poter esportare, ma deve poter eseguire i report.
    PRINT "== 3. Modifica del ruolo impiegato =="
    impiegato = Revoca(impiegato, 4)
    impiegato = Concedi(impiegato, 2)
    PRINT "nuova maschera: "; impiegato; "  (esporta: "; HaPermesso(impiegato, 4); _
          ", esegue: "; HaPermesso(impiegato, 2); ")"
    ' BITTOGGLE inverte un bit: due inversioni riportano al punto di partenza.
    impiegato = BITS.BITTOGGLE(impiegato, 3)
    PRINT "dopo toggle cancellazione: "; HaPermesso(impiegato, 3)
    impiegato = BITS.BITTOGGLE(impiegato, 3)
    PRINT "dopo il secondo toggle:    "; HaPermesso(impiegato, 3)

    ' ── 4. Livello di accesso come numero ────────────────────────────────────
    ' BITGET restituisce il valore del bit (1/0), utile per confronti numerici.
    PRINT "== 4. Confronto numerico =="
    PRINT "admin può cancellare (BITGET): "; BITS.BITGET(admin, 3)
    PRINT "ospite può cancellare (BITGET): "; BITS.BITGET(ospite, 3)
    PRINT "bit fuori maschera: "; BITS.BITGET(ospite, 10)

    ' ── 5. Differenza fra due ruoli ──────────────────────────────────────────
    ' Quali permessi ha l'admin che l'ospite non ha? Si contano i bit del XOR.
    DIM differenza AS LONG
    differenza = BITS.BITCOUNT(admin XOR ospite)
    PRINT "== 5. Differenza admin/ospite =="
    PRINT "permessi che l'ospite non ha: "; differenza
    PRINT "distanza di Hamming fra i due ruoli: "; BITS.HAMMINGDIST(admin, ospite)
END SUB
