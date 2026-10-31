' AI REGISTER (comando) — sorgente HTTP remota: solo registrazione (nessuna chiamata di rete),
' poi AI SET ACTIVE %CHAT + AI.ACTIVE. Verifica la parità del nome attivo fra i 4 backend:
' C# mappa il kind dal source (openai/ollama), il runtime C lo registra con ':' nel source.
' EXPECT: rem-http
IMPORT AI

SUB Main()
    AI REGISTER "rem-http", "completion", "http://127.0.0.1:9/v1", "modello-x"
    AI SET ACTIVE %CHAT, "rem-http"
    PRINT AI.ACTIVE(%CHAT)
END SUB
