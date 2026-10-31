' MiniTest: LoRA L1 — alpha/scaling/target/dropout (parita' 4 backend).
' Senza seed: si asseriscono proprieta' robuste (convergenza, differenza, errore),
' MAI valori esatti di loss.
' EXPECT: c16: 1|c8: 1|diff: 1|badtarget: 1|rs: 1|drop: 1|plus: 1|plusdiff: 1|badratio: 1|dora: 1|doradiff: 1|doramerge: 1|pissa: 1|pissadiff: 1|pissamerge: 1
IMPORT TORCH

SUB Main()
  DIM xA(3) AS DOUBLE = {1, 2, 3, 4}
  DIM yB(3) AS DOUBLE = {4, 3, 2, 1}
  DIM tA AS LONG = TORCH.RESHAPE(TORCH.FROM(xA), 1, 4)
  DIM t2 AS LONG = TORCH.RESHAPE(TORCH.FROM(yB), 1, 4)
  DIM cfg0 AS STRING = "{""vocab"":8,""d_model"":16,""n_layers"":1,""n_heads"":1,""block_size"":8,""rank"":4"
  DIM i AS LONG

  ' alpha 16 (scale 2) vs alpha 8 (scale 1): dinamiche diverse, entrambe convergenti
  DIM l16 AS LONG = TORCH.MODELCREATE("lora", cfg0 + ",""alpha"":16}")
  DIM l8 AS LONG = TORCH.MODELCREATE("lora", cfg0 + ",""alpha"":8}")
  DIM init16 AS DOUBLE = TORCH.MODELLOSS(l16, tA, t2)
  DIM init8 AS DOUBLE = TORCH.MODELLOSS(l8, tA, t2)
  FOR i = 1 TO 40
    TORCH.MODELTRAINSTEP(l16, tA, t2, "adamw", "crossentropy", 0.05)
    TORCH.MODELTRAINSTEP(l8, tA, t2, "adamw", "crossentropy", 0.05)
  NEXT
  DIM end16 AS DOUBLE = TORCH.MODELLOSS(l16, tA, t2)
  DIM end8 AS DOUBLE = TORCH.MODELLOSS(l8, tA, t2)
  IF end16 < init16 THEN PRINT "c16: 1" ELSE PRINT "c16: 0"
  IF end8 < init8 THEN PRINT "c8: 1" ELSE PRINT "c8: 0"
  IF end16 <> end8 THEN PRINT "diff: 1" ELSE PRINT "diff: 0"

  ' target non valido -> MODELCREATE fallisce (handle <= 0), fail-closed
  DIM bad AS LONG = TORCH.MODELCREATE("lora", cfg0 + ",""target"":""xyz""}")
  IF bad <= 0 THEN PRINT "badtarget: 1" ELSE PRINT "badtarget: 0"

  ' rs scaling: gira e resta finito
  DIM lrs AS LONG = TORCH.MODELCREATE("lora", cfg0 + ",""scaling"":""rs""}")
  FOR i = 1 TO 40
    TORCH.MODELTRAINSTEP(lrs, tA, t2, "adamw", "crossentropy", 0.05)
  NEXT
  DIM lrsEnd AS DOUBLE = TORCH.MODELLOSS(lrs, tA, t2)
  IF lrsEnd < 1000000 THEN PRINT "rs: 1" ELSE PRINT "rs: 0"

  ' dropout + target q-only: gira e resta finito
  DIM ldr AS LONG = TORCH.MODELCREATE("lora", cfg0 + ",""dropout"":0.2,""target"":""q""}")
  FOR i = 1 TO 40
    TORCH.MODELTRAINSTEP(ldr, tA, t2, "adamw", "crossentropy", 0.05)
  NEXT
  DIM ldrEnd AS DOUBLE = TORCH.MODELLOSS(ldr, tA, t2)
  IF ldrEnd < 1000000 THEN PRINT "drop: 1" ELSE PRINT "drop: 0"

  ' L3a LoRA+: lr_ratio 16 (B a 16x) vs ratio 1: entrambi convergono, dinamiche diverse
  DIM lp AS LONG = TORCH.MODELCREATE("lora", cfg0 + ",""lr_ratio"":16}")
  DIM initP AS DOUBLE = TORCH.MODELLOSS(lp, tA, t2)
  FOR i = 1 TO 40
    TORCH.MODELTRAINSTEP(lp, tA, t2, "adamw", "crossentropy", 0.05)
  NEXT
  DIM endP AS DOUBLE = TORCH.MODELLOSS(lp, tA, t2)
  IF endP < initP THEN PRINT "plus: 1" ELSE PRINT "plus: 0"
  IF endP <> end8 THEN PRINT "plusdiff: 1" ELSE PRINT "plusdiff: 0"

  ' lr_ratio negativo -> MODELCREATE fallisce, fail-closed
  DIM br AS LONG = TORCH.MODELCREATE("lora", cfg0 + ",""lr_ratio"":-2}")
  IF br <= 0 THEN PRINT "badratio: 1" ELSE PRINT "badratio: 0"

  ' L3b DoRA: converge e diverge da LoRA; merge esatto (il SAVE fonde W+=.. negli stessi
  ' pesi: la loss sullo stesso handle prima/dopo deve coincidere a 6 decimali)
  DIM ld AS LONG = TORCH.MODELCREATE("lora", cfg0 + ",""dora"":1}")
  DIM initD AS DOUBLE = TORCH.MODELLOSS(ld, tA, t2)
  FOR i = 1 TO 40
    TORCH.MODELTRAINSTEP(ld, tA, t2, "adamw", "crossentropy", 0.05)
  NEXT
  DIM endD AS DOUBLE = TORCH.MODELLOSS(ld, tA, t2)
  IF endD < initD THEN PRINT "dora: 1" ELSE PRINT "dora: 0"
  IF endD <> end8 THEN PRINT "doradiff: 1" ELSE PRINT "doradiff: 0"
  DIM mrc AS LONG = TORCH.MODELSAVE(ld, "/tmp/gmb_dora_merge.pt")
  DIM postD AS DOUBLE = TORCH.MODELLOSS(ld, tA, t2)
  IF ROUND(postD, 6) = ROUND(endD, 6) THEN PRINT "doramerge: 1" ELSE PRINT "doramerge: 0"

  ' L3c PiSSA: init SVD (adattatori non zero) invece di B=0; converge, diverge, merge esatto
  DIM lp2 AS LONG = TORCH.MODELCREATE("lora", cfg0 + ",""pissa"":1}")
  DIM initS AS DOUBLE = TORCH.MODELLOSS(lp2, tA, t2)
  FOR i = 1 TO 40
    TORCH.MODELTRAINSTEP(lp2, tA, t2, "adamw", "crossentropy", 0.05)
  NEXT
  DIM endS AS DOUBLE = TORCH.MODELLOSS(lp2, tA, t2)
  IF endS < initS THEN PRINT "pissa: 1" ELSE PRINT "pissa: 0"
  IF endS <> end8 THEN PRINT "pissadiff: 1" ELSE PRINT "pissadiff: 0"
  DIM mrs AS LONG = TORCH.MODELSAVE(lp2, "/tmp/gmb_pissa_merge.pt")
  DIM postS AS DOUBLE = TORCH.MODELLOSS(lp2, tA, t2)
  IF ROUND(postS, 6) = ROUND(endS, 6) THEN PRINT "pissamerge: 1" ELSE PRINT "pissamerge: 0"

  TORCH.MODELFREE(l16) : TORCH.MODELFREE(l8) : TORCH.MODELFREE(lrs) : TORCH.MODELFREE(ldr)
  TORCH.MODELFREE(lp) : TORCH.MODELFREE(br) : TORCH.MODELFREE(ld) : TORCH.MODELFREE(lp2)
END SUB
