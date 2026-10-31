' P1 — `AI OBSERVE SCREEN` (cattura schermo → ContextObject).
' Copre entrambi i percorsi:
'   1. cattura RIUSCITA: la cartella di uscita viene creata dal runtime (come fa il percorso C con
'      mkdir -p) → l'osservazione è registrata (delta = 1), con `AI.OBSERVATIONFILE$` = path e
'      hash FNV non vuoto;
'   2. cattura FALLITA (path in una cartella non creabile): il comando solleva un errore ESPLICITO
'      catturabile con ON ERROR (prima ritornava -1 in silenzio: bug chiuso il 2026-09-16) e
'      l'osservazione NON viene registrata (delta = 0).
' EXPECT: shot= bin/obs_screen.png|delta=  1|file= bin/obs_screen.png|hash= si|err= catturato|delta2=  0
IMPORT AI

SUB Main()
    DIM c0 AS DOUBLE
    DIM c1 AS DOUBLE
    DIM r AS STRING

    ' 1) cattura riuscita (la cartella bin/ viene creata dal runtime se manca)
    r = COMPUTER.SCREENSHOT("bin/obs_screen.png")
    PRINT "shot= "; r

    c0 = AI.OBSERVATIONCOUNT()
    AI OBSERVE SCREEN "bin/obs_screen.png"
    c1 = AI.OBSERVATIONCOUNT()
    PRINT "delta= "; c1 - c0
    PRINT "file= "; AI.OBSERVATIONFILE$(c1)
    IF LEN(AI.OBSERVATIONHASH$(c1)) > 0 THEN PRINT "hash= si" ELSE PRINT "hash= no"

    ' 2) cattura fallita → errore ESPLICITO catturabile (mai silenzioso)
    ON ERROR GOTO H
    AI OBSERVE SCREEN "/nonexistent_dir_gmbasic_xyz/x.png"
    PRINT "err= non-catturato"
    GOTO D
H:
    PRINT "err= catturato"
D:
    PRINT "delta2= "; AI.OBSERVATIONCOUNT() - c1
END SUB
