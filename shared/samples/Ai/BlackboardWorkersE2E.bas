' FILE: samples/Ai/BlackboardWorkersE2E.bas
' ==============================================================================
' E2E Blackboard (Fase 2 — GMBasic-Agent-Runtime-Async-DAG-Verifier-Blackboard):
' N goroutine GPM (GO SUB ... IN wg) scrivono al Blackboard in CONCORRENZA;
' il main le attende con WAIT (park GPM nativo, mai blocco di un worker su
' Task.Wait) e aggrega i risultati leggendo le chiavi.
'
' NB (hazard noto, vedi Bugs/(partial) - AgentBatch-AiAgentSwarm-Conflitto-GPM):
' NON usare AI.BLACKBOARDWAIT DENTRO una goroutine — WAIT è bloccante e un worker
' GPM non deve mai bloccarsi. Qui l'attesa è il WAITGROUP nativo del linguaggio.
' ==============================================================================

IMPORT AI WITH ALIAS

SUB Producer(id AS INTEGER, wg AS WAITGROUP)
    DIM v AS DOUBLE
    v = id * id
    DIM d AS DOUBLE
    ' ogni goroutine scrive una chiave DISTINTA (concorrenza reale sul blackboard)
    d = AI.BLACKBOARDWRITE("bb_" + STR(id), STR(v))
END SUB

SUB Main
    DIM wg AS WAITGROUP
    DIM i AS INTEGER
    FOR i = 1 TO 4
        GO SUB Producer(i, wg) IN wg
    NEXT i

    ' join: blocca SOLO il main finché tutte le goroutine sono terminate (park GPM)
    WAIT wg

    ' synth: il main legge le chiavi e aggrega (somma dei quadrati 1..4 = 30)
    DIM tot AS DOUBLE
    FOR i = 1 TO 4
        tot = tot + VAL(AI.BLACKBOARDREAD("bb_" + STR(i)))
    NEXT i

    PRINT "TOT="; tot
    IF tot = 30 THEN
        PRINT "E2E-OK"
    ELSE
        PRINT "E2E-FAIL"; tot
    END IF
END SUB
