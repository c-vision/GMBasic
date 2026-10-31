' EXPECT: bg-not-cancelled|child-cancelled|root-not-cancelled
IMPORT CONCURRENCY
IMPORT CONTEXT
SUB Main()
  DIM root = CONTEXT.BACKGROUND()
  DIM child = CONTEXT.WITHCANCEL(root)
  IF CONTEXT.CANCELLED(root) = 0 THEN PRINT "bg-not-cancelled"
  CONTEXT.CANCEL(child)
  IF CONTEXT.CANCELLED(child) <> 0 THEN PRINT "child-cancelled"
  IF CONTEXT.CANCELLED(root) = 0 THEN PRINT "root-not-cancelled"
END SUB
