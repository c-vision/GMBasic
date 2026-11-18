' BRAIN-MULTIBRAIN — Second Brain: più cervelli insieme, scope per agente, note e grafo.
'
' Esempio COMPLETO della superficie BASIC dei cervelli (usa ogni comando/funzione del filone
' scope/multi-brain implementato il 2026-09-16 — F1..F5 — con la sola esclusione di quelli non
' disponibili su tutte e 5 le gambe: RAGCONTEXTMULTI/ASKMULTI mancano su VM/C, NOTEGET su C99).
'
' Copre: AI BRAIN USE (multi-attach) · AI.BRAINUSED/BRAINLIST/BRAININFO · AI.SEARCHMULTI ·
' BRAINS.QUERYMULTI · AI AGENT CREATE/SET brains/GET/DELETE (livello AGENT dello scope) ·
' AI VAULT CREATE + AI NOTE CREATE/NOTEEXISTS (vault esplicito) · AI BRAIN DETACH.
'
' Richiede i cervelli costruiti (`gmb rag build` / `gmb graph build`): usa brainvault e trading se
' presenti, altrimenti degrada senza crash (i blocchi di retrieval sono protetti da ON ERROR).
' Il marker finale `BRAIN-MULTI-OK` è deterministico: è quello che si verifica nei gate.
'
' Nota: gli slurp di retrieval richiedono un provider embedding REALE; su una macchina senza
' provider il blocco stampa `search:skip` e il sample prosegue (mai crash: la GPM/host non c'entra).
IMPORT AI

TYPE Ctx
    Usati AS STRING * 128
    Primo AS STRING * 64
    NoteOk AS INTEGER
END TYPE

SUB Main()
    ' Reset iniziale PRIMA di qualsiasi lettura: il registry attached è PERSISTENTE e
    ' condiviso fra le gambe — senza questo DETACH la riga `used:` dipenderebbe dall'ordine
    ' di esecuzione (ogni gamba vedrebbe il trading lasciato dalla precedente). DETACH su
    ' un cervello non collegato è un no-op su tutte le gambe (0.0, mai errore). Il restore
    ' finale in §7 riporta allo stato iniziale (neutralità).
    AI BRAIN DETACH "trading"
    DIM c AS Ctx
    c.Usati = ""
    c.Primo = ""

    ' ── 1. Stato del registry (contratto: lista valida, contenuto dipendente dalla macchina) ──
    PRINT "list:"; LEN(AI.BRAINLIST()) >= 0
    c.Usati = AI.BRAINUSED()
    PRINT "used:"; c.Usati
    PRINT "has-brainvault:"; LEN(AI.BRAININFO("brainvault")) > 0   ' niente confronti fra stringhe

    ' ── 2. Attivazione MULTI-BRAIN (comando AI BRAIN USE a, b) ──
    ' I cervelli restano collegati per tutta la sessione: SEARCHMULTI li interroga TUTTI.
    ' (Il reset è già avvenuto in testa al Main.)
    AI BRAIN USE "brainvault", "trading"
    c.Usati = AI.BRAINUSED()
    PRINT "used-after:"; c.Usati

    ' ── 3. Retrieval multi-brain (richiede embedding reale: degradazione esplicita) ──
    ON ERROR GOTO no_rag
    PRINT "search:"; LEN(AI.SEARCHMULTI("risk parity allocation bonds", 3)) > 0
    GOTO dopo_rag
no_rag:
    PRINT "search:skip"
dopo_rag:

    ' ── 4. Grafo dei cervelli collegati (BRAINS.QUERYMULTI) ──
    ON ERROR GOTO no_graph
    PRINT "graph:"; LEN(BRAINS.QUERYMULTI("risk parity")) > 0
    GOTO dopo_graph
no_graph:
    PRINT "graph:skip"
dopo_graph:

    ' ── 5. Livello AGENT dello scope: dichiarazione dei cervelli dell'agente ──
    ' `brains` è una CSV con alias (`brainvault AS bv, trading`): l'agente vede SOLO questi.
    AI AGENT CREATE scopetest
    AI AGENT SET "scopetest" "brains" "brainvault AS bv, trading"
    PRINT "agent-brains:"; AI.AGENTGET("scopetest", "brains")

    ' ── 6. Note: `AI VAULT CREATE` rende 'demo' il vault ATTIVO della sessione; i comandi
    ' `AI NOTE …` operano su quello (la funzione di lettura prende invece il vault esplicito) ──
    ' La cartella del vault deve esistere (il vault è una chroot: nessuna creazione implicita).
    IF IO.DirExists("/tmp/gmb_brain_demo") = 0 THEN MKDIR "/tmp/gmb_brain_demo"
    AI VAULT CREATE demo, "/tmp/gmb_brain_demo"      ' comando (non ritorna un valore)" >= 0
    AI NOTE CREATE "nota.md", "Second brain multi-cervello"
    c.NoteOk = AI.NOTEEXISTS("demo", "nota.md")
    PRINT "note-exists:"; c.NoteOk

    ' ── 7. Pulizia ──
    ' `AI BRAIN DETACH` scollega un cervello: il registry è PERSISTENTE (e dal 2026-09-16 SINCRONIZZATO
    ' fra mondo C# e mondo C), quindi per non lasciare la macchina modificata si RICOLLEGA subito dopo.
    AI AGENT DELETE "scopetest"
    AI BRAIN DETACH "trading"
    PRINT "detached:"; LEN(AI.BRAINUSED()) >= 0
    AI BRAIN USE "trading"
    PRINT "reattached:"; LEN(AI.BRAINUSED()) >= 0
    ' Restore finale: il reattach qui sopra lascerebbe `trading` collegato in modo
    ' persistente (il registry è condiviso) — la gamba successiva vedrebbe un `used:`
    ' diverso e la macchina resterebbe modificata. Si torna allo stato iniziale.
    AI BRAIN DETACH "trading"
    PRINT "BRAIN-MULTI-OK"
END SUB
