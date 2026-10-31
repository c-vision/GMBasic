' ModelCompilerRecover.bas — §9.B: il "training reale" del quality gate.
' Composizione gate -> retrain -> re-eval -> ACCEPT/ROLLBACK: se il gate rifiuta un modello
' (es. dopo una surgery/prune), recupera con un fine-tune LoRA, lo fonde e lo ri-misura.
'
' Pipeline COMPLETA:
'   1) costruisce un dataset di calibrazione (jsonl {"prompt","response"}) -> <out>.train.bin/.hold.bin
'   2) LLM POLICY RECOVER <model>, <dataset>, <ref>:
'        gate(ref,model) -> se REJECT: fine-tune LoRA -> merge -> re-gate -> ACCEPT/ROLLBACK
'   3) stampa il report JSON (verdict, trained, recovered/rolled_back, gate).
'
' Richiede due dir MLX (trainable): il modello da recuperare e il riferimento.
' Metti il modello in models/model_mlx/ e il riferimento in models/ref_mlx/ (come il resto
' della pipeline del model compiler). Eseguito con modelli reali: il gate iniziale e il re-gate
' misurano la ppl via engine MLX.
IMPORT LLM
IMPORT IO

SUB Main()
  DIM modelDir AS STRING = "models/model_mlx"     ' dir MLX da recuperare (trainable)
  DIM refDir   AS STRING = "models/ref_mlx"       ' dir MLX di riferimento (modello "buono")
  DIM tok      AS STRING = "models/model_mlx/tokenizer.json"

  ' 1) Dataset di calibrazione: una riga JSON per esempio (prompt + risposta attesa).
  DIM ds AS STRING = "work/recover.jsonl"
  DIM h AS LONG = IO.FCREATE(ds)
  IO.FWRITE(h, "{""prompt"": ""La capitale dell'Italia e"", ""response"": ""Roma.""}" + CHR(10))
  IO.FWRITE(h, "{""prompt"": ""Quanto fa due piu due?"", ""response"": ""Quattro.""}" + CHR(10))
  IO.FWRITE(h, "{""prompt"": ""Il sole sorge a"", ""response"": ""est.""}" + CHR(10))
  IO.FWRITE(h, "{""prompt"": ""L'acqua bolle a"", ""response"": ""cento gradi.""}" + CHR(10))
  IO.FCLOSE(h)

  ' Tokenizza + finestra -> work/recover.train.bin + work/recover.hold.bin (holdout derivato).
  TRAIN DATASET BUILD ds, "work/recover", {tokenizer: tok}
  PRINT "dataset pronto: work/recover.train.bin"

  ' 2) Recupero. Se il modello e' gia' entro soglia da ref -> nessun training (trained:false);
  '    altrimenti LoRA fine-tune + merge in <model>.recovered + re-gate.
  LLM POLICY RECOVER modelDir, "work/recover.train.bin", refDir

  ' 3) Il report JSON e' il "verdict" del quality gate (vedi l'output del comando sopra).
  PRINT "recover:done"
END SUB
