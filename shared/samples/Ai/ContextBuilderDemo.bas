' ============================================================================
' ContextBuilderDemo.bas — §3.6 PARSEC: context builder a SEGMENTI per gli AGENTI.
'
' Dimostra i 6 comandi del namespace AI (lato AGENTE):
'   AI CONTEXT CREATE <agent>                 ' azzera lo store di segmenti
'   AI CONTEXT ADD <agent>, <kind>, <testo>   ' kind = SYSTEM | HISTORY | TOOL | OTHER
'   AI CONTEXT MAP <agent>                    ' report della mappa (segmenti + chars + budget)
'   AI CONTEXT TRIM <agent>                   ' elimina i segmenti non-SYSTEM più VECCHI fino al budget
'   AI CONTEXT GC <agent>                     ' eviction: duplicati (kind+text) + HISTORY oltre gli ultimi 8
'   AI CONTEXT COMMIT <agent>                 ' la vista curata = segmenti (SYSTEM per primi) -> AI.CONTEXTVIEW$
'
' Il context builder è la controparte AGENTE del modello-lato `LLM TENSOR CONTEXTMAP/TRIM/GC/COMMIT`
' (namespace LLM). Nasce dal §3.6 PARSEC. Parità 4 backend (IL/PCodeVM/VM-C/C99Native).
' ============================================================================
IMPORT AI
IMPORT LLM

SUB Main()
  DIM agent AS STRING = "analista"

  ' Budget in caratteri della vista curata (policy default: compact).
  AI CONTEXT SET BUDGET agent, 40

  ' 1) CREATE: azzera lo store.  2) ADD: costruisci il contesto a segmenti
  ' (SYSTEM pinned, HISTORY/TOOL evictable — l'ultimo HISTORY è un DUPLICATO).
  AI CONTEXT CREATE agent
  AI CONTEXT ADD agent, "SYSTEM",  "[ruolo: analista] "
  AI CONTEXT ADD agent, "HISTORY", "turno-1: domanda "
  AI CONTEXT ADD agent, "TOOL",    "tool: risultato "
  AI CONTEXT ADD agent, "HISTORY", "turno-1: domanda "

  ' 3) MAP (prima), 4) GC (rimuove il duplicato), 5) MAP, 6) TRIM (al budget), MAP.
  AI CONTEXT MAP agent
  AI CONTEXT GC agent
  AI CONTEXT MAP agent
  AI CONTEXT TRIM agent
  AI CONTEXT MAP agent

  ' 7) COMMIT: vista curata (SYSTEM per primi, poi gli altri) normalizzata.
  AI CONTEXT COMMIT agent
  PRINT "vista curata = ["; AI.CONTEXTVIEW$(agent); "]"

  ' 8) CACHE (version-hash): il hash della vista è la chiave della prefix-KV; VALID la invalida.
  PRINT "hash = "; AI.CONTEXTHASH$(agent)
  PRINT "valid(rev) = "; AI.CONTEXTVALID(agent, AI.CONTEXTHASH$(agent))
  PRINT "valid(estraneo) = "; AI.CONTEXTVALID(agent, "rev=0000000000000000")

  ' 9) METRICHE TASK: i 5 valori standard come JSON (registrati via AI.METRICRECORD).
  AI.METRICRECORD(agent, "tokens", 128)
  AI.METRICRECORD(agent, "ttft", 15)
  AI.METRICRECORD(agent, "tool_calls", 2)
  AI.METRICRECORD(agent, "llm_calls", 1)
  AI.METRICRECORD(agent, "latency", 220)
  PRINT "metrics = "; AI.TASKMETRICS$(agent)

  ' 10) FILEOUTLINE (namespace LLM): i simboli definiti in QUESTO file (il programma analizza sé).
  PRINT "outline = "; LLM.FILEOUTLINE("ContextBuilderDemo.bas")

  PRINT "context-builder: OK"
END SUB
