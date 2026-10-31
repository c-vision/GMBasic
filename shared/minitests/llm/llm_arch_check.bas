' EXPECT: LLM ARCH CHECK 'qwen3_moe': 9 tensori, 0 senza ruolo (OTHER)
' MiniTest: LLM ARCH CHECK su fixture con DESCRITTORE REALE (model_type=qwen3_moe): i tensori
' dell'index hanno tutti un ruolo dal `.gmbarch` dell'ABI di LLMEnv.
' NOTA (2026-10-29): la risoluzione legge SOLO la cartella ABI di LLMEnv (`paths.abiDir`), non
' piu' `<modello>/arch/*.gmbarch` — i descrittori per-modello sotto samples/Fixtures non sono
' piu' consultati (una sola sede: due copie dello stesso descrittore divergevano).
IMPORT LLM
SUB Main()
  LLM ARCH CHECK "../../samples/Fixtures/models/tiny_hf"
END SUB
