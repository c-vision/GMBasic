' MiniTest: LLM KERNEL SHOW <dir> -> primitive (kernel) dichiarate dalle strutture della descrizione.
' EXPECT: LLM KERNEL 'tiny_arch': 2 strutture|attention    type=mha              prim=-|norm         type=rmsnorm          prim=-
IMPORT LLM
SUB Main()
  LLM KERNEL SHOW "../../samples/Fixtures/models/tiny_arch"
END SUB
