' MiniTest: orchestrazione (B3) — TEAM DAG (AGoT) sul runtime C condiviso.
' Deterministico (nessun modello): usa il fallback dei brain/agenti. Verifica che la strategia
' DAG (decomposizione → sotto-nodi → sintesi) giri senza errori su tutti i backend.
' EXPECT: run-ok
IMPORT AI

SUB Main
  AI AGENT CREATE Decomp
  AI AGENT CREATE W1
  AI AGENT CREATE W2
  AI TEAM CREATE T
  AI TEAM T ADD AGENT Decomp
  AI TEAM T ADD AGENT W1
  AI TEAM T ADD AGENT W2
  AI TEAM T SET COORDINATOR Decomp
  AI TEAM T SET STRATEGY DAG
  AI TEAM T SET DAG 1, 2, 2
  AI TEAM T RUN "Spiega cosa e' un DAG."
  PRINT "run-ok"
END SUB
