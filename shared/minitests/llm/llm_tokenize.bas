' LLM TOKENIZE/DETOKENIZE/VOCAB/TOKENIZER/TEMPLATE/TEMPLATETEST — alias TOP-LEVEL (B2).
' Riscritti nel parser a `LLM TENSOR <verbo>` (stesso dispatch, 4 backend). Verbo = 1° argomento.
' EXPECT: LLM TENSOR TOKENIZE '../../samples/Fixtures/ai/tiny_tokenizer.json': n=11 ids=74,71,78,78,81,223,89,81,84,78,70|LLM TENSOR DETOKENIZE '../../samples/Fixtures/ai/tiny_tokenizer.json': n=11 text=hello world|LLM TENSOR VOCAB '../../samples/Fixtures/ai/tiny_tokenizer.json': vocab_size=259|LLM TENSOR TOKENIZER '../../samples/Fixtures/ai/tiny_tokenizer.json': vocab_size=259 source=gpt2|LLM TENSOR TEMPLATE '../../samples/Fixtures/models/tiny_chat':|{{ bos_token }}{% for message in messages %}{{ message['role'] }}: {{ message['content'] }}{% endfor %}{{ eos_token }}|LLM TENSOR TEMPLATETEST '../../samples/Fixtures/models/tiny_chat':|system: You are helpfuluser: Hi
IMPORT LLM
SUB Main()
  LLM TOKENIZE     "../../samples/Fixtures/ai/tiny_tokenizer.json", "hello world"
  LLM DETOKENIZE   "../../samples/Fixtures/ai/tiny_tokenizer.json", "74,71,78,78,81,223,89,81,84,78,70"
  LLM VOCAB        "../../samples/Fixtures/ai/tiny_tokenizer.json"
  LLM TOKENIZER    "../../samples/Fixtures/ai/tiny_tokenizer.json"
  LLM TEMPLATE     "../../samples/Fixtures/models/tiny_chat"
  LLM TEMPLATETEST "../../samples/Fixtures/models/tiny_chat", "system:You are helpful|user:Hi"
END SUB
