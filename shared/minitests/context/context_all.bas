' MiniTest NAMESPACE CONTEXT — tutte le funzioni della superficie.
' Derivato dai MiniTest per-funzione (stessi input e oracoli).
' EXPECT: bg-not-cancelled|child-cancelled|root-not-cancelled|done-ok
IMPORT CONCURRENCY
IMPORT CONTEXT

SUB T_ctx1()
  DIM root = CONTEXT.BACKGROUND()
  DIM child = CONTEXT.WITHCANCEL(root)
  IF CONTEXT.CANCELLED(root) = 0 THEN PRINT "bg-not-cancelled"
  CONTEXT.CANCEL(child)
  IF CONTEXT.CANCELLED(child) <> 0 THEN PRINT "child-cancelled"
  IF CONTEXT.CANCELLED(root) = 0 THEN PRINT "root-not-cancelled"
END SUB

SUB T_ctx2()
  DIM root = CONTEXT.BACKGROUND()
  DIM ch = CONTEXT.DONE(root)
  IF ch <> 0 THEN PRINT "done-ok"
END SUB

SUB Main()
  T_ctx1()
  T_ctx2()
END SUB
