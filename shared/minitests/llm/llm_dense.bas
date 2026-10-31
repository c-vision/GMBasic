' MiniTest: LLM TENSOR DENSE — da un MoE estrae UN esperto per layer e lo materializza come FFN denso
' (variante "split-mixtral" di antirez/gguf-tools, MIT): router rimosso, `*_exps` -> `ffn_{gate,up,down}`.
' Il 3° argomento è la spec degli esperti ("0" = esperto 0 per tutti i layer; "0132.." = una cifra per layer).
' EXPECT: LLM TENSOR DENSE: in=../../samples/Fixtures/models/tiny_moe.gguf out=work/dense.gguf spec=0|OK
IMPORT LLM

SUB Main()
  ON ERROR GOTO H
  LLM TENSOR DENSE "../../samples/Fixtures/models/tiny_moe.gguf", "work/dense.gguf", "0"
  PRINT "OK"
  GOTO D
H:
  PRINT "ERR"
D:
END SUB
