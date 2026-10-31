' EXPECT: LLM POLICY (role -> formato):|(nessuna regola esplicita: si usano i default)|default: ROUTED_EXPERT->q4k  FFN->q4_0  EMBEDDING->f16  ATTENTION->q8_0  NORM->f32
' MiniTest: LLM POLICY SHOW -> regole di formato per ruolo (default, senza alcun SET).
' NOTA (2026-10-29): l'oracolo elencava i default come se fossero regole esplicite
' ("FFN -> q4_0"); lo SHOW stampa invece la riga `default:` unica + l'avviso di nessuna
' regola: allineato l'EXPECT all'output (identico su tutte e 4 le gambe).
IMPORT LLM
SUB Main()
  LLM POLICY SHOW
END SUB
