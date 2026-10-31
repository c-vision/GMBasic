' MiniTest: L11 lettore imatrix — parita' 4 backend.
' File sintetico in formato imatrix (righe: layer expert count <energie...>).
' layer0: (1+2+3+4+5+6+7+8)/8 = 4.5 ; layer1: 10 -> rank map [2,8] (min 2 max 8).
' EXPECT: n:  2|e0:  4.5|e1:  10|r0:  2|r1:  8|bad: 1|lora: 1
IMPORT TORCH
IMPORT IO

SUB Main()
  DIM p AS STRING = "/tmp/gmb_l11_imatrix.txt"
  DIM h AS LONG = IO.FCREATE(p)
  IO.FWRITE(h, "# synthetic imatrix" + CHR(10))
  IO.FWRITE(h, "0 0 1 1 2 3 4" + CHR(10))
  IO.FWRITE(h, "0 1 1 5 6 7 8" + CHR(10))
  IO.FWRITE(h, "1 0 1 10 10 10 10" + CHR(10))
  IO.FCLOSE(h)

  DIM e AS LONG = TORCH.IMATRIXLOAD(p)
  DIM sh() = TORCH.SHAPE(e)
  PRINT "n: "; sh(0)
  PRINT "e0: "; TORCH.GET(e, 0, 0)
  PRINT "e1: "; TORCH.GET(e, 1, 0)

  DIM r AS LONG = TORCH.IMATRIXRANKS(p, 2, 8)
  PRINT "r0: "; INT(TORCH.GET(r, 0, 0))
  PRINT "r1: "; INT(TORCH.GET(r, 1, 0))

  DIM bad AS LONG = TORCH.IMATRIXLOAD("/tmp/gmb_l11_non_esiste.txt")
  IF bad <= 0 THEN PRINT "bad: 1" ELSE PRINT "bad: 0"

  ' L11: rank per-layer nel LoRA (cfg ranks) — converge con rank diversi per layer
  DIM xA(3) AS DOUBLE = {1, 2, 3, 4}
  DIM yB(3) AS DOUBLE = {4, 3, 2, 1}
  DIM tA AS LONG = TORCH.RESHAPE(TORCH.FROM(xA), 1, 4)
  DIM t2 AS LONG = TORCH.RESHAPE(TORCH.FROM(yB), 1, 4)
  DIM cfg AS STRING = "{""vocab"":8,""d_model"":16,""n_layers"":2,""n_heads"":1,""block_size"":8,""rank"":4,""alpha"":8,""ranks"":[2,8]}"
  DIM m AS LONG = TORCH.MODELCREATE("lora", cfg)
  DIM i0 AS DOUBLE = TORCH.MODELLOSS(m, tA, t2)
  DIM i AS LONG
  FOR i = 1 TO 40
    TORCH.MODELTRAINSTEP(m, tA, t2, "adamw", "crossentropy", 0.05)
  NEXT
  IF TORCH.MODELLOSS(m, tA, t2) < i0 THEN PRINT "lora: 1" ELSE PRINT "lora: 0"
  TORCH.MODELFREE(m) : TORCH.MODELFREE(tA) : TORCH.MODELFREE(t2)

  TORCH.MODELFREE(e) : TORCH.MODELFREE(r)
  ON ERROR RESUME NEXT
  KILL p
  ON ERROR GOTO 0
END SUB
