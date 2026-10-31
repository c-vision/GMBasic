' MiniTest: LLM TENSOR LIST <dir> -> ruoli + formato proposto dalla descrizione .gmbarch risolta.
' EXPECT: LLM TENSOR 'tiny_arch': 4 ruoli|embed                    -> EMBEDDING        f16|attn.wkv                 -> ATTN_KV          q4_k|experts.                 -> ROUTED_EXPERT    iq2_xxs|norm                     -> NORM             f32
IMPORT LLM
SUB Main()
  LLM TENSOR LIST "../../samples/Fixtures/models/tiny_arch"
END SUB
