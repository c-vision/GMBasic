' FILE: Ai/AgentSwarmGoroutines.bas
' =================================================================================================
'       SWARM DI AGENTI SU GOROUTINE GPM GREZZE (GO SUB + CHANNEL + WAITGROUP)
' =================================================================================================
'
' AgentSwarmAsync.bas mostra la via "alta" (AI.AGENTSWARM, concorrenza gestita dall'helper).
' Questo sample mostra la via "bassa": costruire lo swarm a mano con i primitivi GPM del
' linguaggio (GO SUB / CHANNEL / WAITGROUP), lo stesso pattern Worker Pool di
' Concurrency/GoRoutinesWorkers.bas ma con AI.AGENTRUN al posto del calcolo.
'
' REGOLA IMPORTANTE (bug 2026-09-08, vedi vault Bugs/BackendC99-GoroutineChannel-Send-Senza-
' Receive-Hang): un canale GMBasic senza capacità esplicita può essere unbuffered a seconda del
' backend (rendezvous puro, parità Go — SEND si blocca finché non c'è un RECEIVE pronto). Ogni
' SEND deve avere un RECEIVE corrispondente da qualche parte, altrimenti il worker resta
' bloccato per sempre. Qui Main riceve esattamente N risultati (uno per worker) — mai un SEND
' "orfano".
' =================================================================================================

IMPORT AI

SUB Worker(NomeAgente AS STRING, TaskText AS STRING, Ris AS CHANNEL OF STRING)
    DIM R AS STRING
    R = AI.AGENTRUN(NomeAgente, TaskText)
    SEND R TO Ris
END SUB

SUB Main()
    PRINT "--- AgentSwarmGoroutines: swarm di agenti su goroutine GPM grezze ---"

    DIM W AS WAITGROUP
    DIM Ris AS CHANNEL OF STRING
    DIM N AS INTEGER
    N = 4

    ' Crea gli agenti e lancia un worker per ciascuno (fire-and-forget, coordinato dal WAITGROUP).
    DIM I AS INTEGER
    FOR I = 0 TO N - 1
        AI AGENT CREATE "w" + STR$(I)
        GO SUB Worker("w" + STR$(I), "riassumi in una riga: cos'e' una coda FIFO", Ris) IN W
    NEXT I

    ' Riceve esattamente N risultati — drena il canale, mai un SEND senza RECEIVE.
    DIM Risultato AS STRING
    FOR I = 0 TO N - 1
        RECEIVE Risultato FROM Ris
        PRINT "Agente w"; I; ": "; Risultato
    NEXT I

    WAIT W
    PRINT "--- Swarm completato, tutti i worker terminati ---"
END SUB
