' EXPECT: LLM TENSOR EXECPLAN 'bin/skip.gguf' -> 'bin/skip.json' (arch=qwen3_moe layers=2 weights=656, kv/token=32 skip=1)
' MiniTest: LLM TENSOR EXECPLAN — PRODUTTORE DI SKIP (§9.A M10/CED): un layer con TUTTI i tensori
' ALL-ZERO (via ABLATE) viene marcato {"skip":true} nel piano per-layer. Parità 4 backend.
IMPORT LLM
SUB Main()
  MKDIR "bin"
  LLM MODEL ABLATE "../../samples/Fixtures/models/tiny_moe.gguf", "bin/skip.gguf", "blk.0."
  LLM TENSOR EXECPLAN "bin/skip.gguf", "bin/skip.json"
END SUB
