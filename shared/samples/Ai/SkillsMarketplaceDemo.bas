' PARITY-EXCLUDE: rete (download marketplace: conteggi 181 vs 161 vs 20 anche tra gambe C# identiche) (2026-09-23, Parita-5-Backend-Residui §2B; TEMPDIR con slash allineato VM-C nello stesso change)
' FILE: AI/SkillsMarketplaceDemo.bas
' ======================================================================================================================
' AI SKILLS MARKETPLACE — scarica un marketplace di skills in una cartella temporanea,
' conta le skill trovate e cancella la cartella temporanea.
' ======================================================================================================================
' Scopo: dimostrare l'importazione di un marketplace di skills (es. github.com/anthropics/skills)
' in una cartella temporanea, contando le skill rilevate dallo scanner (SKILL.md) prima di
' eliminare la cartella. L'archivio skills restante è quello configurato (AiConfig.SkillPaths).
'
' NOTE:
'   - Richiede `git` installato e accesso alla rete.
'   - TEMPDIR() ritorna la cartella temporanea del sistema.
'   - Le skill del marketplace NON vengono copiate nell'archivio: restano nella cartella
'     temporanea e vengono eliminate con essa.
' ======================================================================================================================
IMPORT AI WITH ALIAS
IMPORT IO WITH ALIAS

SUB Main()
    DIM base$ AS STRING
    DIM dir$ AS STRING
    DIM sk() AS STRING

    ' 1. Path temporaneo per il marketplace
    base$ = TEMPDIR()
    dir$ = base$ + "gmb-skills-marketplace"

    ' 2. Crea la cartella temporanea (MKDIR è un COMANDO legacy: niente parentesi, niente
    '    forma qualificata — AGENTS.md §1/§2.0).
    MKDIR dir$
    PRINT "cartella temp: "; dir$

    ' 3. Scarica il marketplace nella cartella temporanea (destinazione esplicita)
    AI SKILLS MARKETPLACE "https://github.com/anthropics/skills", dir$
    PRINT "marketplace: scaricato"

    ' 4. Conta le skill trovate nel marketplace (via l'indice rigenerato)
    '    Nota: AI.SKILLSLIST() elenca le skill dei percorsi configurati (incluso il marketplace
    '    appena aggiunto). Per contare solo quelle del marketplace, leggiamo l'indice aggiornato.
    sk = AI.SKILLSLIST()
    PRINT "skill totali (archivio + marketplace): "; UBOUND(sk) + 1

    ' 5. Rimuove il percorso del marketplace (il clone è in <dir>/repo) dalla config,
    '    leggendo i path configurati e cancellando la cartella temporanea
    DIM paths() AS STRING
    DIM j AS INTEGER
    paths = AI.SKILLSPATHS()
    FOR j = 0 TO UBOUND(paths)
        IF INSTR(paths(j), "gmb-skills-marketplace") > 0 THEN
            AI PATH SKILL REMOVE paths(j)
        END IF
    NEXT j
    RMDIR dir$, TRUE
    PRINT "cartella temp eliminata"

    ' 6. Verifica: le skill tornano quelle dell'archivio configurato
    sk = AI.SKILLSLIST()
    PRINT "skill archivio: "; UBOUND(sk) + 1

    PRINT
    PRINT "Test completato."
END SUB
