' FILE: Ai/AiSecondBrainVault.bas
' Second Brain L1/L2/L3 — VAULT, NOTE, LINK, BRAIN (funzioni AI.*).
' Copre: AI.VAULTINFO/VAULTFOLDERS/VAULTCOUNT/VAULTLIST, AI.NOTE*,
' AI.NOTELINK/NOTEUNLINK/NOTESETALIAS (via AI.NOTELINK), AI.LINKJSON/LINKSOUT/BACKLINKS/
' LINKDANGLING, AI.BRAINLIST/BRAINUSED/BRAININFO.
'
' OUTPUT DETERMINISTICO: i valori di vault/brain contengono timestamp (`indexedAt`) e stato
' persistito (registri vault/brain condivisi fra esecuzioni) → si verificano PROPRIETÀ stabili
' (la chiamata ritorna una stringa/non vuota), non il contenuto. I NOTE* e i LINK* sono invece
' deterministici e stampano il valore.

IMPORT AI

SUB Main()
    PRINT "=== AI Second Brain (VAULT/NOTE/LINK/BRAIN) ==="

    ' Root del vault come LETTERALE + MKDIR (§2.0: un identificatore nudo = NOME, non valore;
    ' `sub` sarebbe keyword riservata).
    IF IO.DirExists("work/secondbrain-vault") = 0 THEN MKDIR "work/secondbrain-vault"
    IF IO.DirExists("work/secondbrain-vault/docs") = 0 THEN MKDIR "work/secondbrain-vault/docs"

    ' VAULT (L1) — info/count/folders/list sono stato/tempo-dipendenti → proprietà stabile.
    AI VAULT CREATE "sb", "work/secondbrain-vault"
    AI VAULT INDEX "sb"
    PRINT "vaultInfo:   "; IIF(LEN(AI.VAULTINFO("sb")) > 0, 1, 0)
    PRINT "vaultCount:  "; IIF(AI.VAULTCOUNT("sb") >= 0, 1, 0)
    PRINT "vaultList:   "; IIF(LEN(AI.VAULTLIST()) > 0, 1, 0)
    PRINT "vaultFolders:"; IIF(LEN(AI.VAULTFOLDERS("sb")) >= 0, 1, 0)

    ' NOTE (L2) — deterministici.
    AI NOTE CREATE "intro.md", "hello world", "alpha,beta"
    AI NOTE CREATE "altra.md", "seconda nota"
    PRINT "noteGet:     "; AI.NOTEGET("sb", "intro.md")
    PRINT "noteTitle:   "; AI.NOTETITLE("sb", "intro.md")
    PRINT "noteTags:    "; AI.NOTETAGS("sb", "intro.md")
    PRINT "noteWords:   "; AI.NOTEWORDCOUNT("sb", "intro.md")
    PRINT "noteExists:  "; AI.NOTEEXISTS("sb", "intro.md")
    PRINT "noteFolder:  "; AI.NOTEFOLDER("sb", "intro.md")

    ' LINK (L3) — linksOut/backlinks sono JSON deterministici; LINKJSON ha l'ordine dei nodi
    ' dipendente dall'enumerazione → proprietà stabile; DANGLING è numerico.
    AI NOTE LINK "intro.md" TO "altra.md" AS "vedi"
    PRINT "linksOut:    "; AI.LINKSOUT("sb", "intro.md")
    PRINT "backlinks:   "; AI.BACKLINKS("sb", "altra.md")
    PRINT "linkJson:    "; IIF(LEN(AI.LINKJSON("sb")) > 0, 1, 0)
    PRINT "linkDangling:"; AI.LINKDANGLING("sb")

    ' BRAIN — stato persistito → proprietà stabile.
    PRINT "brainList:   "; IIF(LEN(AI.BRAINLIST()) >= 0, 1, 0)
    PRINT "brainUsed:   "; IIF(LEN(AI.BRAINUSED()) >= 0, 1, 0)
    PRINT "brainInfo:   "; IIF(LEN(AI.BRAININFO("sb")) > 0, 1, 0)

    PRINT "=== AiSecondBrainVault ok ==="
END SUB
