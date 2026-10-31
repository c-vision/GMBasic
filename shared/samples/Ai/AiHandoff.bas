' AI.HANDOFF / HANDOFFCOMPLETE / HANDOFFSTATUS / HANDOFFLIST / HANDOFFCLEAR
' Orchestrazione agenti (A9): Handoff — trasferimento di un task a un agente destinatario,
' con stato (pending/done) e risultato. Runtime condiviso: HandoffRegistry (C#) / gmb_handoff.c.
IMPORT AI

SUB Main
  DIM ok AS DOUBLE
  ok = AI.HANDOFF("refactor", "coder")
  ok = AI.HANDOFF("review", "reviewer")
  PRINT "refactor: "; AI.HANDOFFSTATUS("refactor")
  ok = AI.HANDOFFCOMPLETE("refactor", "completato")
  PRINT "dopo complete: "; AI.HANDOFFSTATUS("refactor")
  PRINT AI.HANDOFFLIST()
  ok = AI.HANDOFFCLEAR()
  PRINT "handoff rimossi: "; ok
END SUB
