' FILE: AI/AiEmbeddedRegisterRouting.bas
' ======================================================================================================================
' GUIDA TEORICA: REGISTER con modelli EMBEDDED, REMOTE e ROUTING a crediti
' ======================================================================================================================
' Questo programma mostra come registrare provider AI di origine diversa con la sintassi
' unificata AI REGISTER e come impostare il credit-based routing.
'
' 1. LE ORIGINI DEI MODELLI (source)
'    Sintassi (alias-first): AI REGISTER alias, capability, source [, param]
'    - EMBEDDED  (hf:<repo>): modello locale scaricato da HuggingFace (llamacpp/mlx/GGUF).
'      Esempio: AI REGISTER("llm-local", "completion", "hf:ggml-org/qwen3.8-27b-q8_0", "")
'    - EMBEDDED  (path locale): cartella con modello già scaricato, auto-detected.
'      Esempio: AI REGISTER("llm-local", "completion", "/opt/models/qwen", "")
'    - REMOTO    (openai/anthropic/ollama...): endpoint API remoto con apiKey nel config JSON.
'      Esempio: AI REGISTER("openai-demo", "completion", "openai", "{""apiKey"":""sk-demo"",""model"":""gpt-4o-mini""}")
'    - EMBEDDING (capability embedding): ruolo separato per i modelli vettoriali.
'      Esempio: AI REGISTER("emb-local", "embedding", "hf:modelscope/bge-m3", "")
'
' 2. CAPABILITY NUMERICA
'    - %CHAT/%OCR/%AUDIO (0/1/2) sono costanti predefinite usabili ovunque l'argomento
'      atteso è la capability/ruolo (es. il secondo di AI REGISTER). ACTIVE(%CHAT) legge
'      l'attivo chat.
'
' 3. CREDIT-BASED ROUTING
'    - AI SET CREDITS nome, N imposta un budget di crediti gratuiti (-1 = illimitato).
'    - AI.CREDITS(nome) legge i crediti rimasti.
'    - A ogni completamento i crediti calano; a 0 il provider va BLOCKED e ROTATE/ACTIVE
'      passano automaticamente al successivo (per priorità, free-first).
'
' 4. NOTA SULL'ESECUZIONE
'    - Il VM/C non ha ancora DISCOVER/SETCREDITS/CREDITS: usa il backend IL nativo (-n).
'    - Le variabili stringa sono LOCALI (le costanti stringa globali non sono ancora
'      supportate dal backend IL nativo).
' ======================================================================================================================

IMPORT AI WITH ALIAS

SUB Main()
    PRINT "REGISTER: modelli embedded, remoti ed embedding"
    PRINT "==============================================="
    PRINT

    ' --- 1. Modello locale embedded (da HuggingFace) ---
    AI REGISTER "llm-local", "completion", "hf:ggml-org/qwen3.8-27b-q8_0", ""
    AI SET PRIORITY "llm-local", 100

    ' --- 2. Modello remoto (OpenAI) ---
    AI REGISTER "openai-demo", "completion", "openai", "{""apiKey"":""sk-demo"",""model"":""gpt-4o-mini""}"
    AI SET PRIORITY "openai-demo", 50

    ' --- 3. Modello embedding locale ---
    AI REGISTER "emb-local", "embedding", "hf:modelscope/bge-m3", ""

    ' --- 4. Attiva il modello locale per la chat ---
    AI SET ACTIVE "completion", "llm-local"
    PRINT "Provider chat attivo: "; AI.ACTIVE(%CHAT)
    PRINT

    ' --- 5. Credit-based routing: budget 5, poi BLOCKED e rotazione ---
    DIM nome AS STRING
    nome = AI.ACTIVE(%CHAT)
    AI SET CREDITS nome, 5
    PRINT "Crediti per "; nome; ": "; AI.CREDITS(nome)
    PRINT

    ' Simula completamenti (in uso reale RecordCompletion decrementa da solo).
    DIM i AS INTEGER
    FOR i = 1 TO 6
        IF i <= 5 THEN
            AI SET CREDITS nome, 5 - i
            PRINT "  completamento "; i; " -> crediti "; AI.CREDITS(nome)
        ELSE
            AI SET CREDITS nome, 0
            PRINT "  completamento "; i; " -> crediti "; AI.CREDITS(nome); " (BLOCKED)"
        END IF
    NEXT i
    PRINT
    PRINT "Stato "; nome; ": "; AI.STATUS(nome)
    PRINT "Successivo (rotazione): "; AI.ACTIVE(%CHAT)
    PRINT

    ' --- 6. Lista provider per ruolo ---
    PRINT "Provider completion registrati: "; AI.LIST("completion")
    PRINT "Provider embedding registrati: "; AI.LIST("embedding")

    PRINT
    PRINT "Test completato."
END SUB
