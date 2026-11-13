' MiniTest: LLM KV SHOW <dir> -> metadati RUNTIME (engine) dichiarati: state / kv.dtype / activation.dtype.
' EXPECT: LLM KV 'tiny_arch': state=- kv.dtype=- activation.dtype=-
' NB: le tre voci vengono dal DESCrittore `.gmbarch` e la ricerca e' SOLO in LLMEnv/ABI/
' (la cartella per-modello `<model>/arch/` e' stata rimossa: vedi llmdesc_resolve_core).
' `tiny_arch` e' un archetipo di FIXTURE e non ha un descrittore ABI -> i campi sono "-"
' su tutte le gambe (deterministico).
IMPORT LLM
SUB Main()
  LLM KV SHOW "../../samples/Fixtures/models/tiny_arch"
END SUB
