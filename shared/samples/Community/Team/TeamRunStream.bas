' FILE: Team/TeamRunStream.bas
' =================================================================
' TEAM RUN START/WAIT + ONSTREAM — API di orchestrazione del team.
'
' Deterministico (nessun provider richiesto): team/handle inesistenti
' producono un errore esplicito, mai un risultato silenzioso.
'   - AI.TEAMRUNSTART(team, task) → avvia il run; team mancante → errore esplicito.
'   - AI.TEAMRUNWAIT(handle)      → risultato del run; handle ignoto → errore esplicito.
'   - AI.TEAMONSTREAM(sub)        → registra la callback di stream; idempotente, sempre 1.
' =================================================================
IMPORT AI

SUB Main()
    PRINT "=== TEAM Run/Stream ==="

    ' Team inesistente → errore esplicito.
    DIM start$ AS STRING = AI.TEAMRUNSTART("team-inesistente", "task")
    PRINT "start: "; start$

    ' Handle inesistente → errore esplicito (mai risultato silenzioso).
    DIM wait$ AS STRING = AI.TEAMRUNWAIT("handle-inesistente")
    PRINT "wait: "; wait$

    ' Registrazione callback di stream: idempotente, sempre 1.
    ' NB: confronto invece di PRINT numerico diretto (il formato STR$ di un double
    ' diverge fra IL e VM/C: "1 " vs "1").
    DIM streamOn = AI.TEAMONSTREAM("on-member")
    IF streamOn = 1 THEN
        PRINT "stream= 1"
    ELSE
        PRINT "stream= ERR"
    END IF

    PRINT "done"
END SUB
