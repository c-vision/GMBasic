' FILE: Concurrency/AsyncHelpers.bas
' =================================================================================================
' CONCURRENCY.SLEEPASYNC / CONCURRENCY.TASKYIELD — helper asincroni (GPM).
' =================================================================================================
' AWAIT CONCURRENCY.SLEEPASYNC(ms)  → sospende il task (park-based) per ms millisecondi
' AWAIT CONCURRENCY.TASKYIELD()     → cede il turno ad altri task (nessuna attesa)
' Si usano dentro ASYNC FUNCTION/SUB con AWAIT.
' =================================================================================================
IMPORT CONCURRENCY

ASYNC FUNCTION Lavoro(id AS INTEGER) AS STRING
    AWAIT CONCURRENCY.SLEEPASYNC(20)
    AWAIT CONCURRENCY.TASKYIELD()
    RETURN "task-" + STR(id) + "-done"
END FUNCTION

SUB Main()
    CLS
    PRINT "CONCURRENCY.SLEEPASYNC / TASKYIELD"
    PRINT "==================================="
    PRINT

    PRINT "Prima dell'attesa"
    AWAIT CONCURRENCY.SLEEPASYNC(50)
    PRINT "Dopo SLEEPASYNC(50)"

    AWAIT CONCURRENCY.TASKYIELD()
    PRINT "Dopo TASKYIELD"

    DIM r1 = AWAIT Lavoro(1)
    DIM r2 = AWAIT Lavoro(2)
    PRINT r1
    PRINT r2
END SUB
