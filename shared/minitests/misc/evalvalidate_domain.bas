' MiniTest: AI EVAL VALIDATE (T4.1) — UAT via vault (oracolo a termini).
' EXPECT: ok:{"ok":true,"passed":["- The system must support user login","- It must calculate the order total"],"failed":[],"coverage":1}|bad:{"ok":false,"passed":["- It must calculate the order total"],"failed":["- The system must support user login"],"coverage":0.429}|done
IMPORT AI

SUB Main
  DIM j AS STRING
  AI EVAL VALIDATE "the system must support user login and it must calculate the order total" AGAINST VAULT "../../samples/Fixtures/UatVault" INTO j
  PRINT "ok:"; j
  AI EVAL VALIDATE "the component calculates the order total but the authentication flow is missing" AGAINST VAULT "../../samples/Fixtures/UatVault" INTO j
  PRINT "bad:"; j
  PRINT "done"
END SUB
