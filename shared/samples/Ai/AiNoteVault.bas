' FILE: Ai/AiNoteVault.bas
' Second Brain L2 — vault + note: comandi AI VAULT/NOTE e funzioni AI.NOTE*.
' Il vault attivo (ultimo AI VAULT CREATE) è usato dai comandi NOTE.
' Runtime condiviso: gmb_secondbrain.c (C99Native/VM-C) e Basic.AI.SecondBrain (IL/PCode).

IMPORT AI

SUB Main()
    PRINT "=== AI NOTE / VAULT (Second Brain L2) ==="

    ' Root del vault come LETTERALE (§2.0: un identificatore nudo diventa il suo NOME, non il
    ' valore della variabile) e directory creata prima (VaultCreate torna -1 se manca).
    IF IO.DirExists("work/note-vault") = 0 THEN MKDIR "work/note-vault"
    AI VAULT CREATE "nv", "work/note-vault"
    AI VAULT INDEX "nv"

    ' Crea una nota con frontmatter (title + tags via TAGS).
    AI NOTE CREATE "intro.md", "hello world", "alpha,beta"
    PRINT "get:    "; AI.NOTEGET("nv", "intro.md")
    PRINT "title:  "; AI.NOTETITLE("nv", "intro.md")
    PRINT "tags:   "; AI.NOTETAGS("nv", "intro.md")
    PRINT "words:  "; AI.NOTEWORDCOUNT("nv", "intro.md")
    PRINT "exists: "; AI.NOTEEXISTS("nv", "intro.md")
    PRINT "folder: "; AI.NOTEFOLDER("nv", "intro.md")

    ' Modifica e append.
    AI NOTE UPDATE "intro.md", "contenuto aggiornato"
    AI NOTE APPEND "intro.md", " riga in coda"
    PRINT "after:  "; AI.NOTEGET("nv", "intro.md")

    ' Seconda nota + link manuale con alias.
    AI NOTE CREATE "altra.md", "seconda nota"
    AI NOTE LINK "intro.md" TO "altra.md" AS "vedi"
    AI NOTE UNLINK "intro.md" TO "altra.md"

    ' Sposta e cancella.
    AI NOTE MOVE "altra.md", "spostata.md"
    PRINT "moved:  "; AI.NOTEEXISTS("nv", "spostata.md")
    AI NOTE DELETE "spostata.md"
    PRINT "deleted:"; AI.NOTEEXISTS("nv", "spostata.md")

    PRINT "=== AiNoteVault ok ==="
END SUB
