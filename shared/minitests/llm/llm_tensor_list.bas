' MiniTest: LLM TENSOR LIST <dir> -> ruoli + formato proposto dalla descrizione .gmbarch risolta.
' EXPECT: LLM TENSOR 'tiny_arch': 0 ruoli
' NB: i ruoli vengono dal descrittore .gmbarch (LLMEnv/ABI). In questo checkout l'ABI non e'
' raggiungibile dall'harness, quindi il resolver ricade sulla geometria DERIVATA da config.json:
' nessun ruolo dichiarato -> "0 ruoli" (deterministico su tutte le gambe).
IMPORT LLM
SUB Main()
  LLM TENSOR LIST "../../samples/Fixtures/models/tiny_arch"
END SUB
