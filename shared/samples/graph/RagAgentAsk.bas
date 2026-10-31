' AI.AGENTRUNRAG — l'agente interroga i brain RAG del Second Brain (2026-09-07).
' Il loop agente usa i tool rag.* (search/context/ask) per rispondere a una domanda GROUNDED
' sul contenuto dei vault (retrieval vettoriale multi-brain con provenance), non sulla
' struttura del grafo (per quello c'è AI.AGENTRUNGRAPH). Il dispatch dei tool è nativo C#
' verso MultiBrainSearch (sola lettura; nessun codice BASIC nel GPM).
'
' NOTA: richiede (a) almeno un brain RAG collegato (AI BRAIN USE <nome>) e (b) un provider di
' completamento che faccia TOOL-CALLING. Con la fixture-completion il loop non produce
' tool_calls e risponde senza interrogare i brain — il sample COMPILA comunque (Conformance).
IMPORT AI WITH ALIAS

SUB Main()
  ' 1) agente
  AI AGENT CREATE "rag-agent"

  ' 2) loop agente sul RAG: il modello usa rag.search/rag.context per fondare la risposta
  DIM answer$ AS STRING
  answer$ = AI.AGENTRUNRAG("rag-agent", _
    "Cerca nei brain collegati cosa dice la strategia risk parity e riassumi i punti chiave " + _
    "citando le fonti.", 5)
  PRINT "=== RISPOSTA ==="
  PRINT answer$

  AI AGENT DELETE "rag-agent"
  PRINT "done"
END SUB
