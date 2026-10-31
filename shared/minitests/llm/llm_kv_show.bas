' MiniTest: LLM KV SHOW <dir> -> metadati RUNTIME (engine) dichiarati: state / kv.dtype / activation.dtype.
' EXPECT: LLM KV 'tiny_arch': state=attention_kv kv.dtype=bf16 activation.dtype=f16
IMPORT LLM
SUB Main()
  LLM KV SHOW "../../samples/Fixtures/models/tiny_arch"
END SUB
