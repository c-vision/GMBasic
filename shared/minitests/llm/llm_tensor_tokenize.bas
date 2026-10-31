' LLM TENSOR TOKENIZE / DETOKENIZE / VOCAB / TOKENIZER — B2 (superficie tokenizer)
' Wiring su llmtoken (fonte unica). Sotto-verbi di LLM TENSOR (nessun nuovo TokenKind).
' EXPECT: LLM TENSOR TOKENIZE '../../samples/Fixtures/ai/tiny_tokenizer.json': n=11 ids=74,71,78,78,81,223,89,81,84,78,70|LLM TENSOR DETOKENIZE '../../samples/Fixtures/ai/tiny_tokenizer.json': n=11 text=hello world|LLM TENSOR TOKENIZER '../../samples/Fixtures/ai/tiny_tokenizer.json': vocab_size=259 source=gpt2|LLM TENSOR VOCAB '../../samples/Fixtures/ai/tiny_tokenizer.json': vocab_size=259
IMPORT LLM
SUB Main()
  LLM TENSOR TOKENIZE   "../../samples/Fixtures/ai/tiny_tokenizer.json", "hello world"
  LLM TENSOR DETOKENIZE "../../samples/Fixtures/ai/tiny_tokenizer.json", "74,71,78,78,81,223,89,81,84,78,70"
  LLM TENSOR TOKENIZER  "../../samples/Fixtures/ai/tiny_tokenizer.json"
  LLM TENSOR VOCAB      "../../samples/Fixtures/ai/tiny_tokenizer.json"
END SUB
