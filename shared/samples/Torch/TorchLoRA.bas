' ============================================================================
' TorchLoRA.bas — LoRA su un piccolo transformer via TORCH (arch="lora")
' 1) BASE transformer addestrato su taskA (1→2→3→4→1) e salvato in work/base.pt
' 2) LoRA su base (rank 4): fine-tune su taskB (1,2,3,4 → 4,3,2,1), trainabili
'    SOLO gli adattatori A/B (base invariata)
' 3) MODELSAVE sul modello lora scrive il MERGE (W += BA) base-only → il file è
'    ricaricabile come arch="transformer" (verificato nel round-trip nativo)
' ============================================================================
IMPORT TORCH
IMPORT IO WITH ALIAS

SUB Main()
  DIM dir AS STRING = PROJECTFOLDER() + "/work"
  IF NOT IO.DIREXISTS(dir) THEN MKDIR dir

  DIM xA(3) AS DOUBLE = {1, 2, 3, 4}
  DIM yA(3) AS DOUBLE = {2, 3, 4, 1}
  DIM yB(3) AS DOUBLE = {4, 3, 2, 1}
  DIM tA AS LONG = TORCH.RESHAPE(TORCH.FROM(xA), 1, 4)
  DIM t1 AS LONG = TORCH.RESHAPE(TORCH.FROM(yA), 1, 4)
  DIM t2 AS LONG = TORCH.RESHAPE(TORCH.FROM(yB), 1, 4)
  DIM cfg AS STRING = "{""vocab"":8,""d_model"":32,""n_layers"":2,""n_heads"":2,""block_size"":8}"

  ' 1) base su taskA
  DIM b AS LONG = TORCH.MODELCREATE("transformer", cfg)
  DIM i AS LONG
  FOR i = 1 TO 120
    TORCH.MODELTRAINSTEP(b, tA, t1, "adamw", "crossentropy", 0.02)
  NEXT
  PRINT "base taskA ="; TORCH.MODELLOSS(b, tA, t1)
  PRINT "base taskB ="; TORCH.MODELLOSS(b, tA, t2)
  TORCH.MODELSAVE(b, dir + "/base.pt")

  ' 2) LoRA su base, fine-tune taskB
  DIM lcfg AS STRING = "{""vocab"":8,""d_model"":32,""n_layers"":2,""n_heads"":2,""block_size"":8,""base"":""" + dir + "/base.pt"",""rank"":4}"
  DIM l AS LONG = TORCH.MODELCREATE("lora", lcfg)
  PRINT "lora taskB init ="; TORCH.MODELLOSS(l, tA, t2)
  FOR i = 1 TO 200
    TORCH.MODELTRAINSTEP(l, tA, t2, "adamw", "crossentropy", 0.05)
  NEXT
  PRINT "lora taskB end  ="; TORCH.MODELLOSS(l, tA, t2)

  ' 3) merge on save → file base-only (ricaricabile come transformer dal round-trip nativo)
  DIM rc AS LONG = TORCH.MODELSAVE(l, dir + "/merged.pt")
  PRINT "save merged rc  ="; rc
  PRINT "STATE           =" + TORCH.MODELSTATE(l)
  TORCH.MODELFREE(b) : TORCH.MODELFREE(l)
  PRINT "=== TorchLoRA: done ==="
END SUB
