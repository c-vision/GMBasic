' MiniTest — versioni-FUNZIONE delle famiglie LLM e `LLM … INTO` (2026-10-30, seconda ondata).
' LLM POLICY/ARCH/MODULE/KERNEL/KV hanno il comando che STAMPA e la funzione che RESTITUISCE
' l'output catturato (LLM.POLICY/ARCH/MODULE/KERNEL/KV); `LLM … INTO` assegna senza
' stampare. Sorgente unica: gmbllm `llmmodelio_*_cmd_to` + dominio condiviso gmb_llm_cmd_out.
' Un verbo non valido per la famiglia deve dare un ERRORE catturabile (mai stringa vuota).
' EXPECT: policy=SI|arch=SI|into=SI|uguale=SI|err=SI
IMPORT LLM
SUB Main()
  ' 1) funzione: POLICY SHOW restituisce la tabella ruolo→formato
  DIM p AS STRING
  p = LLM.POLICY("SHOW")
  PRINT IIF(INSTR(p, "LLM POLICY") > 0, "policy=SI", "policy=NO")

  ' 2) funzione: ARCH SHOW (nessuna descrizione risolta in ambiente pulito)
  DIM a AS STRING
  a = LLM.ARCH("SHOW")
  PRINT IIF(INSTR(a, "LLM ARCH") > 0, "arch=SI", "arch=NO")

  ' 3) comando con INTO: stesso valore, NESSUNA stampa
  DIM q AS STRING
  LLM POLICY SHOW INTO q
  PRINT IIF(LEN(q) > 0, "into=SI", "into=NO")
  PRINT IIF(q = p, "uguale=SI", "uguale=NO")

  ' 4) verbo non valido → errore catturabile (KV SHOW senza modello)
  DIM ok AS LONG
  ok = 0
  ON ERROR GOTO Bad
  DIM k AS STRING
  k = LLM.KV("SHOW")
  ok = 0
  GOTO Fine
Bad:
  ok = 1
Fine:
  PRINT IIF(ok = 1, "err=SI", "err=NO")
END SUB
