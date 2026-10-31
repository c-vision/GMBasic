' ============================================================================
' TransformerMini.bas — addestra da zero un piccolo transformer (LM) via TORCH
' Estende MODELCREATE a arch="transformer": tok_emb + pos_emb + N layer
' pre-norm (self-attention causale + FFN) + LM head. Loss cross-entropy su
' token (logits [B,S,V] → flatten), ottimizzatore adamw.
' Obiettivo: imparare la sequenza ciclica 1→2→3→4→1 (next-token).
' ============================================================================
IMPORT TORCH

SUB Main()
  DIM xs(3) AS DOUBLE = {1, 2, 3, 4}
  DIM ys(3) AS DOUBLE = {2, 3, 4, 1}
  DIM xa AS LONG = TORCH.RESHAPE(TORCH.FROM(xs), 1, 4)
  DIM ya AS LONG = TORCH.RESHAPE(TORCH.FROM(ys), 1, 4)

  DIM mm AS LONG = TORCH.MODELCREATE("transformer", "{""vocab"":8,""d_model"":32,""n_layers"":2,""n_heads"":2,""block_size"":8}")
  PRINT "PARAMS    ="; TORCH.MODELPARAMETERS(mm)
  PRINT "LOSS init ="; TORCH.MODELLOSS(mm, xa, ya)

  DIM i AS LONG
  FOR i = 1 TO 200
    TORCH.MODELTRAINSTEP(mm, xa, ya, "adamw", "crossentropy", 0.02)
  NEXT

  PRINT "LOSS end  ="; TORCH.MODELLOSS(mm, xa, ya)
  PRINT "STATE     ="; TORCH.MODELSTATE(mm)
  TORCH.MODELFREE(mm)
  PRINT "=== TransformerMini: done (loss decresce verso ~0 = appreso) ==="
END SUB
