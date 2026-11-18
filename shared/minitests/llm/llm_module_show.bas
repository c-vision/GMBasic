' MiniTest: LLM MODULE SHOW <dir> -> strutture (moduli) dichiarate + se il converter le sa mappare.
' EXPECT: LLM ARCH SUPPORT 'tiny_arch':|attention    mha                    NON supportata|norm         rmsnorm                supportata|-> 1 struttura/e non supportata/e
IMPORT LLM
SUB Main()
  LLM MODULE SHOW "../../samples/Fixtures/models/tiny_arch"
END SUB
