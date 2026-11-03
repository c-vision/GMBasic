' FILE: Databases/MagazzinoIsam.bas
' SCENARIO DIDATTICO — magazzino su tabella ISAM indicizzata, con manutenzione.
'
' Un magazzino tiene i prodotti in una tabella con indice sul codice (SKU). A fine
' giornata il responsabile deve: caricare i prodotti nuovi in blocco, ritrovare un
' articolo dal codice, controllare che l'indice sia coerente e ripararlo se serve.
'
' ISAM è la scelta giusta quando serve accesso per CHIAVE (non per posizione come DBF):
' l'indice permette SEEK/lookup su milioni di righe senza scandire il file.
'
' Domini: ISAM + IO + JSON. File di lavoro in work/ (cancellati a fine scenario).
IMPORT ISAM
IMPORT IO
IMPORT JSON

TYPE Prodotto
    SKU AS STRING * 10
    Nome AS STRING * 20
    Giacenza AS DOUBLE
END TYPE

SUB Main()
    MKDIR "work"
    ON ERROR RESUME NEXT
    KILL "work/magazzino.isdb"
    KILL "work/magazzino.idx"
    ON ERROR GOTO 0

    ' ── 1. Apertura con indice sul codice ────────────────────────────────────
    ISAM OPEN "work/magazzino.isdb" OF Prodotto ALIAS M INDEXED BY SKU
    PRINT "== 1. Tabella aperta =="
    PRINT "in uso: "; ISAM.USED("M")
    PRINT "ordine di indice: "; ISAM.INDEXORD()
    PRINT "dimensione record: "; ISAM.RECSIZE(); " byte"

    ' ── 2. Carico in blocco dei prodotti nuovi ───────────────────────────────
    ' APPENDMANY evita un ciclo di APPEND (stessa scrittura, un solo salto).
    DIM nuovi(2) AS Prodotto
    nuovi(0).SKU = "SKU-001" : nuovi(0).Nome = "Farina 1kg"   : nuovi(0).Giacenza = 40
    nuovi(1).SKU = "SKU-002" : nuovi(1).Nome = "Zucchero 1kg" : nuovi(1).Giacenza = 25
    nuovi(2).SKU = "SKU-003" : nuovi(2).Nome = "Burro 250g"   : nuovi(2).Giacenza = 12
    PRINT "== 2. Carico in blocco =="
    PRINT "prodotti inseriti: "; ISAM.APPENDMANY("M", nuovi)
    PRINT "record totali: "; ISAM.LASTREC()

    ' ── 3. Ricerca per chiave ────────────────────────────────────────────────
    ' SEEK posiziona sull'indice: dopo la ricerca FOUND dice se la chiave esiste.
    PRINT "== 3. Ricerca per codice =="
    ISAM SEEK M, "SKU-002"
    PRINT "SKU-002 trovato: "; ISAM.FOUND()
    PRINT "recno: "; ISAM.RECNO()
    ISAM SEEK M, "SKU-999"
    PRINT "SKU-999 trovato: "; ISAM.FOUND()

    ' ── 4. Navigazione e filtro ──────────────────────────────────────────────
    PRINT "== 4. Navigazione =="
    ISAM GO M, TOP
    PRINT "primo record: BOF="; ISAM.BOF(); " EOF="; ISAM.EOF(); " recno="; ISAM.RECNO()
    ISAM GO M, BOTTOM
    PRINT "ultimo record: BOF="; ISAM.BOF(); " EOF="; ISAM.EOF(); " recno="; ISAM.RECNO()
    ' Nessun filtro attivo: la stringa è vuota (si userà SET FILTER per popolarla).
    PRINT "filtro attivo: ["; ISAM.DBFILTER("M"); "]"

    ' ── 5. Manutenzione: integrità e riparazione ────────────────────────────
    ' INTEGRITY restituisce un report JSON: se il file è sano si ripara comunque
    ' (l'operazione è idempotente e consolidа l'indice).
    PRINT "== 5. Manutenzione =="
    DIM report AS STRING
    report = JSON.STRINGIFY(ISAM.INTEGRITY())
    PRINT "report integrità: "; LEN(report); " caratteri"
    ISAM REPAIR -1
    PRINT "riparazione eseguita"
    report = JSON.STRINGIFY(ISAM.INTEGRITY())
    PRINT "record dopo la riparazione: "; ISAM.LASTREC()

    ' ── 6. Blocchi e chiusura ────────────────────────────────────────────────
    ' RLOCK/FLOCK proteggono la riga e il file quando più processi scrivono.
    PRINT "== 6. Blocchi =="
    ISAM GO M, TOP
    PRINT "blocco riga ottenuto: "; (ISAM.RLOCK() <> 0)
    PRINT "blocco file ottenuto: "; (ISAM.FLOCK() <> 0)
    ISAM UNLOCK M
    ISAM CLOSE M

    KILL "work/magazzino.isdb"
    KILL "work/magazzino.idx"
    PRINT "Magazzino aggiornato."
END SUB
