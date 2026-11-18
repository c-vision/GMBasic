' MiniTest: LLM TENSOR MEMORY positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR MEMORY '../../samples/Fixtures/models/tiny_moe.gguf' tensori=25 total_bytes=656 (0.00 GiB)|embedding  32 bytes (4.9%)|experts    192 bytes (29.3%)|router     32 bytes (4.9%)|norm       112 bytes (17.1%)|output     32 bytes (4.9%)|other      256 bytes (39.0%)
IMPORT LLM
SUB Main()
  LLM TENSOR MEMORY "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
