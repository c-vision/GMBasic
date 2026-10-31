' MiniTest: LLM VOCAB con filtro (argomento opzionale) -> vocab_size + match deterministici su 4 backend.
' EXPECT: LLM TENSOR VOCAB '../../samples/Fixtures/ai/tiny_tokenizer.json': vocab_size=259|match=0 (filtro 'he')
IMPORT LLM
SUB Main()
  LLM VOCAB "../../samples/Fixtures/ai/tiny_tokenizer.json", "he"
END SUB
