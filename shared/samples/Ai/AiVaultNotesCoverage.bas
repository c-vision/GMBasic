' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: Ai/AiVaultNotesCoverage.bas
' Copertura funzioni AI VAULT/NOTE/BRAIN/MEMORY (Second Brain L2) — forma funzione
' (AI.VaultXxx/NoteXxx/BrainXxx), complementare ai comandi AI VAULT/NOTE usati in AiNoteVault.bas.
IMPORT AI
IMPORT IO

SUB Main()
    PRINT "=== AI VAULT/NOTE/BRAIN coverage ==="

    DIM v AS STRING = "work/vaultcov"
    IF IO.DirExists(v) = 0 THEN MKDIR v
    ' Path LETTERALE (non la variabile v): il parser C# converte gli identificatori nudi
    ' dei comandi AI nel loro NOME (ParseNameAsString) mentre il parser C valuta la
    ' variabile — con v si registrano due root diverse ("v" vs "work/vaultcov", bug dedicato).
    AI VAULT CREATE "cov", "work/vaultcov"
    AI VAULT INDEX "cov"

    ' --- Vault: info/list/count/folders ---
    ' Il JSON NON si stampa: contiene indexVersion, che AVANZA a ogni esecuzione (17/19/21 fra le
    ' gambe) -> parita' impossibile. Si verifica una proprieta' stabile del JSON.
    DIM vinfo AS STRING = AI.VAULTINFO("cov")
    PRINT "vaultinfo ok:"; IIF(INSTR(vinfo, "indexVersion") > 0, 1, 0)
    PRINT "vaultcount:   "; AI.VAULTCOUNT("cov")
    PRINT "vaultfolders: "; AI.VAULTFOLDERS("cov")
    PRINT "vaultlist:    "; AI.VAULTLIST()

    ' --- Vault folder props ---
    PRINT "addfolder:    "; AI.VAULTADDFOLDER("cov", v, 1)
    PRINT "setprops:     "; AI.VAULTSETFOLDERPROPS("cov", v, 1)
    PRINT "rmfolder:     "; AI.VAULTREMOVEFOLDER("cov", v)

    ' --- Note link/alias/unlink ---
    AI NOTE CREATE "a.md", "prima nota", "x"
    AI NOTE CREATE "b.md", "seconda nota", "y"
    PRINT "notelink:     "; AI.NOTELINK("cov", "a.md", "b.md", "vedi")
    PRINT "notesetalias: "; AI.NOTESETALIAS("cov", "a.md", "b.md", "alias1")
    PRINT "noteunlink:   "; AI.NOTEUNLINK("cov", "a.md", "b.md")

    ' --- Brain ---
    PRINT "brainlist:    "; AI.BRAINLIST()
    PRINT "braininfo:    "; AI.BRAININFO("default")

    ' --- Memoria / RAG / piano / task ---
    PRINT "suggesttags:  "; AI.SUGGESTTAGS("testo di prova con parole chiave")
    PRINT "memoryrank:   "; AI.MEMORYRANK("query")
    PRINT "ragmulti:     "; AI.RAGCONTEXTMULTI("query")
    PRINT "qaanswer:     "; AI.QAANSWER("domanda", "default")
    PRINT "agentfix:     "; AI.AGENTFIX("errore fittizio")
    PRINT "memconsol:    "; AI.MEMORYCONSOLIDATE()
    PRINT "planclear:    "; AI.PLANCLEAR()
    PRINT "taskclear:    "; AI.TASKCLEAR()

    ' --- Sync / ingest ---
    PRINT "syncfolder:   "; AI.SYNCFOLDER("cov", v)
    PRINT "ingestweb:    "; AI.INGESTWEB("cov", "https://example.com")

    PRINT "=== AiVaultNotesCoverage ok ==="
END SUB
