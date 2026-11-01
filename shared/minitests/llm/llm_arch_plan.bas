' EXPECT: LLM ARCH PLAN 'tiny_arch': 5 tensori (1 scale) -> /tmp/gmb_tiny_plan.txt|OTHER              5|destinazioni: text=5
' MiniTest: LLM ARCH PLAN <dir>, <out> -> piano per tensore dalla descrizione + conteggi per ruolo.
IMPORT LLM
SUB Main()
  LLM ARCH PLAN "../../samples/Fixtures/models/tiny_arch", "/tmp/gmb_tiny_plan.txt"
END SUB
