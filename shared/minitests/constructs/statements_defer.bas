' Oracle = golden: DEFER <statement> — eseguito a FINE scope della procedura, LIFO.
' Sintassi canone: samples/Concurrency/PCodeGpmMutexDefer.bas (DEFER MUTEX UNLOCK).
' 2026-09-21: PCode fix (loop infinito, resume errato dopo l'ultimo DEFER); emitter C#
' verificato già corretto (il "placeholder" del doc bug era stale); IL fix (supporto DEFER
' sul path lineare, prima GPM-v2-only — nuovo meccanismo EmitDeferredBodiesIfAny/
' _returnExitLabel condiviso in IlProgramCompiler, percorso GPM v2 non toccato). Vedi AiWiki
' "(solved) - DEFER-Divergenza-IL-PCode-emitter C#.md". 4/4 backend allineati
' (work|defer-1|defer-2|main) — non più escluso dal confronto di parità IL/PCode/VM-C.
' EXPECT: work|defer-1|defer-2|main

SUB Work()
    DEFER PRINT "defer-2"
    DEFER PRINT "defer-1"
    PRINT "work"
END SUB

SUB Main()
  CHDIR PROJECTFOLDER()
    Work()
    PRINT "main"
END SUB
