' FILE: Console/BloccoNote.bas
' SCENARIO DIDATTICO — piccolo blocco note che usa gli appunti come "ponte".
'
' Un programma console prepara una nota (più righe), la copia negli appunti di
' sistema con CLIP SET, la rilegge con CLIP GET per verificare il passaggio, poi
' usa gli appunti anche come memoria "ultima versione copiata" (annulla = ricopia
' la versione precedente) e infine li svuota con CLIP CLEAR.
'
' Domini: CONSOLE (sotto-namespace CLIP) + stringhe. Nessun input interattivo,
' output deterministico.
IMPORT CONSOLE
IMPORT CLIP
IMPORT STRINGS

SUB Main()
    ' ── 1. Composizione della nota ───────────────────────────────────────────
    DIM nota AS STRING
    nota = "Ordine del giorno" + CHR(10)
    nota = nota + "1) approvare il bilancio" + CHR(10)
    nota = nota + "2) fissare la prossima riunione"
    PRINT "== Nota =="
    PRINT nota
    PRINT "righe: "; LEN(nota) - LEN(REPLACE(nota, CHR(10), "")) + 1

    ' ── 2. Copia negli appunti ───────────────────────────────────────────────
    CLIP SET nota
    PRINT "== Copia negli appunti =="
    DIM letto AS STRING
    CLIP GET letto
    PRINT "copia fedele: "; (letto = nota)
    PRINT "caratteri copiati: "; LEN(letto)

    ' ── 3. Gli appunti come "versione precedente" ────────────────────────────
    ' Si modifica la nota, ma prima di sovrascrivere gli appunti si conserva
    ' l'originale: è il classico annulla senza stato extra.
    DIM precedente AS STRING
    CLIP GET precedente
    nota = nota + CHR(10) + "3) preparare le slide"
    CLIP SET nota
    CLIP GET letto
    PRINT "== Versione aggiornata =="
    PRINT "nuova versione negli appunti: "; (INSTR(letto, "slide") > 0)
    PRINT "versione conservata: "; (INSTR(precedente, "slide") = 0)

    ' ── 4. Ricerca dentro la nota ────────────────────────────────────────────
    PRINT "== Ricerca =="
    PRINT "contiene 'bilancio': "; (INSTR(letto, "bilancio") > 0)
    PRINT "contiene 'riunione': "; (INSTR(letto, "riunione") > 0)

    ' ── 5. Svuotamento ───────────────────────────────────────────────────────
    CLIP CLEAR
    CLIP GET letto
    PRINT "== Dopo CLIP CLEAR =="
    PRINT "appunti vuoti: "; (LEN(letto) = 0)
END SUB
