' FILE: Ai/AiBenchmark.bas
' =================================================================
' LLM BENCHMARK + LLM.BENCHRESULT — benchmark della sessione KV di un agente.
'
' Comando (azione):   LLM BENCHMARK <alias>, PREFILL|DECODE|ALL [, {json}]
'   - esegue N iterazioni (json {"iterations":N,"text":"..."}, default 1) di
'     prefill e/o decode sulla sessione KV dell'agente e registra l'ultimo
'     benchmark.
' Funzione (lettura): s$ = LLM.BENCHRESULT(<alias>)
'   - JSON dell'ultimo benchmark registrato:
'       {"context":"<nome>","mode":"ALL","iterations":N,
'        "result":{"prefill":{...},"decode":{...}}}
'   - ogni blocco mode riporta tokens, elapsedMs, avg/min/maxTokPerSec.
'
' Funzioni (lettura): s$ = LLM.PREFILLSTATS(<alias>) / LLM.DECODESTATS(<alias>)
'   - ultima misura singola di prefill/decode:
'       {"context":"<nome>","tokens":N,"elapsedMs":M,"tokPerSec":X}
'   - senza misura registrata -> JSON {"error":"'<nome>' non ha ancora misure di prefill|decode"}.
'
' Comportamento (mai crash, mai risultato silenzioso):
'   - agente non registrato / senza sessione (fixture) -> JSON {"error":"..."}.
'   - modalita' non valida -> errore esplicito (attese PREFILL|DECODE|ALL).
'   - LLM.BENCHRESULT prima di un benchmark -> errore esplicito.
'
' Con un provider locale (llamacpp) configurato, dopo un AIAGENT.ASK la
' sessione dell'agente esiste e il benchmark misura prestazioni reali:
'   AI REGISTER "local", %CHAT, "llamacpp", "{""modelPath"":""/path/to/model.gguf"",""maxTokens"":""64""}"
'   AI SET ACTIVE %CHAT, "local"
' =================================================================
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "LLM BENCHMARK + LLM.BENCHRESULT — sessione KV di un agente"
    PRINT "=========================================================="
    PRINT

    ' 1. LLM.BENCHRESULT prima di qualunque benchmark -> error esplicito.
    DIM err0$ = LLM.BENCHRESULT("nessun-agente")
    PRINT "benchresult senza benchmark: "; err0$
    PRINT

    ' 2. Benchmark su agente non registrato -> error esplicito (non crash).
    LLM BENCHMARK "nessun-agente", PREFILL
    DIM err$ = LLM.BENCHRESULT("nessun-agente")
    PRINT "benchmark agente inesistente: "; err$
    PRINT

    ' 3. Agente registrato ma senza sessione (fixture) -> error esplicito.
    AI AGENT CREATE "bench-agent"
    LLM BENCHMARK "bench-agent", DECODE, {iterations:2}
    DIM noSession$ = LLM.BENCHRESULT("bench-agent")
    PRINT "agente senza sessione: "; noSession$
    PRINT

    ' 4. Se un provider locale e' attivo, un ASK crea la sessione; poi il
    '    benchmark misura prestazioni reali. Senza provider, ASK fallisce
    '    con errore esplicito (gestito da ON ERROR).
    ON ERROR GOTO salta_bench
    AI AGENT SET "bench-agent" "maxtokens" "32"
    DIM risp$ = AI.AGENTASKSYNC("bench-agent", "Rispondi con una sola parola.")
    PRINT "risposta agente: "; risp$
    LLM BENCHMARK "bench-agent", ALL, {iterations:3, text:"Benchmark di prefill e decode."}
    DIM bench$ = LLM.BENCHRESULT("bench-agent")
    PRINT "benchmark: "; bench$
    GOTO fine

salta_bench:
    PRINT "(nessun provider chat attivo — il benchmark reale richiede un modello locale registrato)"

fine:
    AI AGENT DELETE "bench-agent"
    PRINT

    ' 5. Telemetria per-agente (deterministica, nessun provider richiesto):
    '    senza una misura registrata le funzioni ritornano il JSON d'errore
    '    esplicito, identico su tutti e 5 i backend.
    DIM ps$ = LLM.PREFILLSTATS("nessun-agente")
    PRINT "prefillstats senza misura: "; ps$
    DIM ds$ = LLM.DECODESTATS("nessun-agente")
    PRINT "decodestats senza misura: "; ds$
    PRINT

    PRINT "Test completato."
END SUB
