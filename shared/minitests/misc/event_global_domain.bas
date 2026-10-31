' MiniTest: eventi GLOBALI per nome (base dei handler-SUB degli hook).
' ADD HANDLER <evento>, <Sub> + RAISE EVENT <evento>(args) → risoluzione nome→procedura,
' passaggio argomenti e dispatch. Prima del fix IL gli eventi nudi (source=null) erano un no-op.
' EXPECT: handler: task1|done
IMPORT AI

SUB OnBeforeTask(payload AS STRING)
  PRINT "handler: "; payload
END SUB

SUB Main
  ADD HANDLER before_task, OnBeforeTask
  RAISE EVENT before_task("task1")
  PRINT "done"
END SUB
