' MiniTest: L9 MODELEXPORT (merge+save flat; formato validato) — parita' 4 backend.
' Esporta un modello LoRA (fonde gli adattatori) e verifica il file; formato ignoto -> errore.
' EXPECT: exp: 0|file: 1|badfmt: 1
IMPORT TORCH
IMPORT IO

SUB Main()
  DIM xA(3) AS DOUBLE = {1, 2, 3, 4}
  DIM yB(3) AS DOUBLE = {4, 3, 2, 1}
  DIM tA AS LONG = TORCH.RESHAPE(TORCH.FROM(xA), 1, 4)
  DIM t2 AS LONG = TORCH.RESHAPE(TORCH.FROM(yB), 1, 4)
  DIM cfg AS STRING = "{""vocab"":8,""d_model"":16,""n_layers"":1,""n_heads"":1,""block_size"":8}"
  DIM p AS STRING = "/tmp/gmb_l9_model.pt"
  ON ERROR RESUME NEXT
  KILL p
  ON ERROR GOTO 0

  DIM m AS LONG = TORCH.MODELCREATE("lora", cfg + ",""rank"":4}")
  DIM i AS LONG
  FOR i = 1 TO 20
    TORCH.MODELTRAINSTEP(m, tA, t2, "adamw", "crossentropy", 0.05)
  NEXT

  DIM rc AS LONG = TORCH.MODELEXPORT(m, p, "flat")
  IF rc = 0 THEN PRINT "exp: 0" ELSE PRINT "exp: "; rc
  IF IO.FILEEXISTS(p) THEN PRINT "file: 1" ELSE PRINT "file: 0"

  ' formato ignoto -> errore (diverso da 0)
  DIM rc2 AS LONG = TORCH.MODELEXPORT(m, p, "zzz")
  IF rc2 <> 0 THEN PRINT "badfmt: 1" ELSE PRINT "badfmt: 0"

  TORCH.MODELFREE(m) : TORCH.MODELFREE(tA) : TORCH.MODELFREE(t2)
  ON ERROR RESUME NEXT
  KILL p
  ON ERROR GOTO 0
END SUB
