' MiniTest: orchestrazione agenti (B2) — AI VERIFY ... REQUIRE ... con COMPILE nel mondo C.
' L'azione "COMPILE <path>" usa il compilatore C (gmb_compile_bas_file); il REQUIRE è
' valutato dal valutatore runtime (gmb_expr_eval_ex) con RESULT$ = risultato del tentativo.
IMPORT AI

SUB Main
  DIM r AS STRING
  AI VERIFY "COMPILE /Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/AIVerifyFixtureOk.bas" REQUIRE "LEN(RESULT$) > 5" INTO r
  PRINT "result="; r
END SUB
