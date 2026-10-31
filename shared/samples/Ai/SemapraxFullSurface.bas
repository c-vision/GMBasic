' samples/Community/Ai/SemapraxFullSurface.bas
' ─────────────────────────────────────────────────────────────────────────────
' GMB SEMANTIC GRAPH (SEMAPRAX) — SUPERFICIE COMPLETA, namespace `LLM`.
' Usa OGNI funzione/comando della superficie, in un unico programma:
'   read-side : LLM.SYMBOL / IMPACT / CALLMAP / INSPECT / CONTEXT /
'               REVISION / CHECKREV / PATCH / FINDSYMBOL / REPOMAP
'   write-side: LLM PATCH "target","rev" … END PATCH  +  LLM.VERIFY() / LLM.COMMIT()
'
' Il patch è revision-bound: applica la sostituzione del corpo di Greet SOLO se la revisione
' coincide con LLM.REVISION() corrente. VERIFY/COMMIT sono receipt read-only (aggregati).
'
' Output deterministico, identico sui 4 backend (IL / PCode / VM-C / C99Native).
IMPORT LLM

SUB Helper()
END SUB

SUB Worker()
  Helper()
END SUB

SUB Greet()
  PRINT "orig"
END SUB

SUB Main()
  Worker()

  PRINT "-- read-side --"
  PRINT "SYMBOL      "; LLM.SYMBOL("Worker")
  PRINT "IMPACT      "; LLM.IMPACT("Helper")
  PRINT "CALLMAP     "; LLM.CALLMAP("Worker")
  PRINT "INSPECT     "; LLM.INSPECT("Worker")
  PRINT "CONTEXT     "; LLM.CONTEXT("Worker", 2)
  PRINT "REVISION    "; LLM.REVISION()
  PRINT "CHECKREV    "; LLM.CHECKREV("rev=0000000000000000")
  PRINT "PATCH       "; LLM.PATCH("Worker")
  PRINT "FINDSYMBOL  "; LLM.FINDSYMBOL("Work")
  PRINT "REPOMAP     "; LLM.REPOMAP()

  PRINT "-- write-side --"
  LLM PATCH "Greet", "rev=e23ec7622292c6a7"
    PRINT "patched"
  END PATCH
  Greet()
  PRINT "VERIFY      "; LLM.VERIFY()
  PRINT "COMMIT      "; LLM.COMMIT()
END SUB
