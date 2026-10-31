' PARITY-EXCLUDE: nondeterministico: timeMs + comportamento asks (7 vs 3)
' B3 — TEAM DAG su VM/C con modello locale (Llama-3.2-1B-Instruct-4bit via GMB_MLX_MODEL_DIR).
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
  AI TEAM T RUN "Spiega in due punti cosa e' un DAG."
  PRINT "run-ok"
  PRINT "stats="; AI.TEAMRUNTIMESTATS("T")
END SUB
