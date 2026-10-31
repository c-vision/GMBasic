' samples/Community/Ai/SemanticPatchApply.bas
' ─────────────────────────────────────────────────────────────────────────────
' GMB SEMANTIC GRAPH (SEMAPRAX) — lato SCRITTURA: `LLM PATCH "target","rev" … END PATCH`.
'
' Il patch è revision-bound: sostituisce il CORPO della procedura target SOLO se la revisione
' coincide con LLM.REVISION() corrente (call-graph Main->Greet => rev=baa4ff0afd9323a6, stabile).
'   - patch con rev CORRETTA -> APPLICATO (il corpo di Greet diventa "patched")
'   - patch con rev ERRATA   -> RIFIUTATO (no-op: Greet resta "patched")
' `LLM.VERIFY()` / `LLM.COMMIT()` sono i receipt read-only del lifecycle: aggregati su TUTTI i
' patch del programma; con un patch a rev errata -> "rejected".
'
' Output atteso (identico su IL / PCode / VM-C / C99Native):
'   patched
'   patched
'   verify: rejected
'   commit: rejected
'   done
IMPORT LLM

SUB Greet()
  PRINT "original"
END SUB

SUB Main()
  ' 1) revisione corretta -> applica la sostituzione del corpo di Greet.
  LLM PATCH "Greet", "rev=baa4ff0afd9323a6"
    PRINT "patched"
  END PATCH
  Greet()

  ' 2) revisione errata -> patch RIFIUTATO (il corpo di Greet resta quello del punto 1).
  LLM PATCH "Greet", "rev=0000000000000000"
    PRINT "wrong-rev-should-not-appear"
  END PATCH
  Greet()

  ' 3) receipt del lifecycle (aggregato: c'è un patch non verificato -> rejected).
  PRINT LLM.VERIFY()
  PRINT LLM.COMMIT()

  PRINT "done"
END SUB
