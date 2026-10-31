' LLM TENSOR TEMPLATE — B2 (chat template): mostra il template del modello.
' Un template errato cambia l'output a parità di grafo (diagnostica). Wiring su
' chat_template.jinja / tokenizer_config.json (dir) o tokenizer.chat_template (GGUF).
' EXPECT: LLM TENSOR TEMPLATE '../../samples/Fixtures/models/tiny_chat':|{{ bos_token }}{% for message in messages %}{{ message['role'] }}: {{ message['content'] }}{% endfor %}{{ eos_token }}|LLM TENSOR TEMPLATETEST '../../samples/Fixtures/models/tiny_chat':|system: You are helpfuluser: Hi
IMPORT LLM
SUB Main()
  LLM TENSOR TEMPLATE     "../../samples/Fixtures/models/tiny_chat"
  LLM TENSOR TEMPLATETEST "../../samples/Fixtures/models/tiny_chat", "system:You are helpful|user:Hi"
END SUB
