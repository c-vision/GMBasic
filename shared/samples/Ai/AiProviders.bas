' FILE: Samples/AI/AiProviders.bas
' =================================================================================================
'       AI SET MODEL, AI SET KEY, AI SET TEMP, AI SET BASE, AI SET DEFAULT
'       AI.ACTIVE, AI.ROTATE, AI.CREDITS — gestione provider LLM per AI.OCR e AI.TRANSCRIBE
' =================================================================================================
' Sintassi (direttiva): getter = funzione, setter = comando.
'   Comando legacy multi-parola:  AI SET MODEL "openrouter", "gpt-4o"
'   Comando legacy a spazio:      AI SET KEY "deepseek", "sk-xyz789"
'   Funzione (lettura):           model = AI.GETMODEL("openrouter")

IMPORT AI WITH ALIAS

SUB Main()

    ' ── Lista provider ────────────────────────────────────────────────────

    DIM list AS STRING

    list = AI.PROVIDERS()
    PRINT list

    ' ── API Key ───────────────────────────────────────────────────────────

    DIM ok AS STRING

    ' Comando legacy multi-parola (setter)
    AI SET KEY "openrouter", "sk-or-v1-abcd1234"
    ok = AI.GETKEY("openrouter")

    ' Comando legacy (setter)
    AI SET KEY "deepseek", "sk-xyz789"
    PRINT ok

    ' ── Modello ───────────────────────────────────────────────────────────

    DIM model AS STRING

    AI SET MODEL "openrouter", "openai/gpt-4o"
    model = AI.GETMODEL("openrouter")
    PRINT "Modello corrente: ", model

    AI SET MODEL "openrouter", "anthropic/claude-3.5-sonnet"
    model = AI.GETMODEL("openrouter")
    PRINT "Modello dopo SETMODEL: ", model

    ' ── Temperatura ───────────────────────────────────────────────────────

    DIM temp AS STRING

    AI SET TEMP "openrouter", 0.7
    temp = AI.GETTEMP("openrouter")
    PRINT "Temperatura: ", temp

    AI SET TEMP "openrouter", 0.3
    temp = AI.GETTEMP("openrouter")
    PRINT "Temp dopo SETTEMP: ", temp

    ' ── Base URL ──────────────────────────────────────────────────────────

    DIM url AS STRING

    AI SET BASE "openrouter", "https://openrouter.ai/api/v1"
    url = AI.GETBASE("openrouter")
    PRINT "Base URL: ", url

    ' ── Info completa ─────────────────────────────────────────────────────

    DIM info AS STRING

    info = AI.INFO("openrouter")
    PRINT info

    ' ── Provider di default ───────────────────────────────────────────────

    DIM def AS STRING

    AI SET DEFAULT "openrouter"
    def = AI.GETDEFAULT()
    PRINT "Default: ", def

    ' ── Abilita / Disabilita ──────────────────────────────────────────────

    AI ENABLE "deepseek"
    AI DISABLE "deepseek"

    ' ── Salva / Carica ────────────────────────────────────────────────────

    AI SAVE CONFIG
    AI LOAD CONFIG

    ' ── Configurazione rapida (comando legacy / funzione) ──────────────────

    AI SET KEY "openrouter", "sk-or-v1-..."
    AI SET MODEL "openrouter", "openai/gpt-4o"
    AI SET TEMP "openrouter", 0.7
    AI SET DEFAULT "openrouter"
    AI SAVE CONFIG

    PRINT "Provider configurato e salvato."

END SUB
