' FILE: Ai/SecondBrainFullCoverage.bas
' Superficie completa del Second Brain L2 (vault + note) + registry skill + plugin,
' esercitata end-to-end e verificata su 4 backend: parità IL+PCode+VM/C,
' gate driver C99Native.
'
' Copre: AI VAULT CREATE/INDEX; AI NOTE CREATE/UPDATE/APPEND/MOVE/DELETE (comandi statement,
' §2.0 legacy a spazio); AI.NOTEGET/NOTETITLE/NOTETAGS/NOTEWORDCOUNT/NOTEEXISTS/NOTEFOLDER;
' AI.SKILLSLIST/AI.SKILLSPATHS; AI.PLUGINSLIST.
'
' NON copre AI.DISCOVER: esegue una vera richiesta HTTP e su IL/PCode solleva un errore con
' argomenti non reali → non deterministico per la parità (si veda minitests/ai/gen_AI_Discover.bas).
'
' La root del vault è un LETTERALE (§2.0: un identificatore nudo diventa il suo NOME, non il
' valore) e la directory è creata prima (VaultCreate torna -1 se manca).
IMPORT AI

SUB Main()
    PRINT "=== Second Brain full coverage ==="

    IF IO.DirExists("work/sb-full-vault") = 0 THEN MKDIR "work/sb-full-vault"
    AI VAULT CREATE "sbfull", "work/sb-full-vault"
    AI VAULT INDEX "sbfull"

    ' --- NOTE CREATE + letture ---
    AI NOTE CREATE "nota.md", "contenuto iniziale", "alpha,beta"
    PRINT "get:    "; AI.NOTEGET("sbfull", "nota.md")
    PRINT "title:  "; AI.NOTETITLE("sbfull", "nota.md")
    PRINT "tags:   "; AI.NOTETAGS("sbfull", "nota.md")
    PRINT "words:  "; AI.NOTEWORDCOUNT("sbfull", "nota.md")
    PRINT "exists: "; AI.NOTEEXISTS("sbfull", "nota.md")
    PRINT "folder: "; AI.NOTEFOLDER("sbfull", "nota.md")

    ' --- UPDATE + APPEND (frontmatter PRESERVATO) ---
    AI NOTE UPDATE "nota.md", "contenuto aggiornato"
    AI NOTE APPEND "nota.md", " + coda"
    PRINT "after:  "; AI.NOTEGET("sbfull", "nota.md")
    PRINT "tags2:  "; AI.NOTETAGS("sbfull", "nota.md")

    ' --- MOVE + DELETE ---
    AI NOTE MOVE "nota.md", "spostata.md"
    PRINT "moved:  "; AI.NOTEEXISTS("sbfull", "spostata.md")
    AI NOTE DELETE "spostata.md"
    PRINT "deleted:"; AI.NOTEEXISTS("sbfull", "spostata.md")

    ' --- SKILLS / PLUGINS: proprietà stabile ---
    ' NB: `LEN(<array-returning>)` NON è confrontabile in modo assoluto (su C# è il conteggio
    ' elementi, sul mondo C la lunghezza della stringa `[a, b]`), e la LISTA è stato-dipendente
    ' (i marketplace aggiungono skill). Si verifica solo la non-vuotezza (> 0), vera in entrambi.
    PRINT "skills:  "; IIF(LEN(AI.SKILLSLIST()) > 0, 1, 0)
    PRINT "spaths:  "; IIF(LEN(AI.SKILLSPATHS()) > 0, 1, 0)
    PRINT "plugins: "; IIF(LEN(AI.PLUGINSLIST()) > 0, 1, 0)

    PRINT "=== SecondBrainFullCoverage ok ==="
END SUB
