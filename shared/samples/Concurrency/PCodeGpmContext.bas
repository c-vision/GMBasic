' PCodeVM: CONTEXT (parità Go context.Context) — cancellazione propagata.
' Un worker ascolta su ctx.Done() (canale chiuso alla cancellazione): Main cancella il
' contesto derivato (WITHCANCEL) e il worker termina. Verifica anche CANCELLED e il fatto
' che un figlio derivato da un parent cancellato nasce già cancellato.
IMPORT CONCURRENCY
IMPORT CONTEXT

SUB Worker(ctx AS CONTEXT, wg AS WAITGROUP)
  DIM done AS CHANNEL
  DIM x AS INTEGER
  DIM ok AS BIT
  done = CONTEXT.DONE(ctx)
  RECEIVE x FROM done OK ok
  IF ok = 0 AND CONTEXT.CANCELLED(ctx) = 1 THEN
    PRINT "PCODE-GPM-CONTEXT-OK"
  ELSE
    PRINT "FAIL:ok="; ok
  END IF
END SUB

SUB Main()
  DIM root AS CONTEXT
  DIM child AS CONTEXT
  DIM grandchild AS CONTEXT
  DIM wg AS WAITGROUP
  root = CONTEXT.BACKGROUND()
  child = CONTEXT.WITHCANCEL(root)
  grandchild = CONTEXT.WITHCANCEL(child)
  GO SUB Worker(grandchild, wg) IN wg
  CONTEXT.CANCEL(child)
  WAIT wg
  IF CONTEXT.CANCELLED(root) = 0 AND CONTEXT.CANCELLED(grandchild) = 1 THEN
    PRINT "PCODE-GPM-CONTEXT-PROPAGATION-OK"
  ELSE
    PRINT "FAIL:PROPAGATION"
  END IF
END SUB
