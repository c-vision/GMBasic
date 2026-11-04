' FILE: Ai/AiBrainGaps.bas
' Copre AI.BRAININSTALL (2 argomenti: name, source opzionale) e AI.BRAINUNINSTALL (1).
' Il runtime risponde con un messaggio esplicito quando il cervello non è dichiarato
' nel manifest: nessun finto successo. Output deterministico.
'
' FIRME (StdLib.AI.cs):
'   BrainInstall(name[, source]) -> LONG
'   BrainUninstall(name)         -> LONG

IMPORT AI

SUB Main()
    PRINT "BRAININSTALL(manuale)   = "; AI.BRAININSTALL("manuale")
    PRINT "BRAINUNINSTALL(manuale) = "; AI.BRAINUNINSTALL("manuale")
    PRINT "AiBrainGaps: completato."
END SUB
