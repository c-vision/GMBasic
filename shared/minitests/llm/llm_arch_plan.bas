' EXPECT: LLM ARCH PLAN 'tiny_arch': 5 tensori (1 scale) -> /tmp/gmb_tiny_plan.txt|EMBEDDING          1|ATTN_KV            2|ROUTED_EXPERT      1|NORM               1|destinazioni: text=5
' MiniTest: LLM ARCH PLAN <dir>, <out> -> piano per tensore dalla descrizione + conteggi per ruolo.
IMPORT LLM
SUB Main()
  LLM ARCH PLAN "../../samples/Fixtures/models/tiny_arch", "/tmp/gmb_tiny_plan.txt"
END SUB
