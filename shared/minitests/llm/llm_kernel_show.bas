' MiniTest: LLM KERNEL SHOW <dir> -> primitive (kernel) dichiarate dalle strutture della descrizione.
' EXPECT: LLM KERNEL 'tiny_arch': 1 strutture|norm         type=rmsnorm          prim=rmsnorm
IMPORT LLM
SUB Main()
  LLM KERNEL SHOW "../../samples/Fixtures/models/tiny_arch"
END SUB
