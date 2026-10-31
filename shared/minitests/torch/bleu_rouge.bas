' MiniTest: TORCH.BLEU + TORCH.ROUGE (L7 Training, parita' 4 backend).
' Variante GMBasic: token [a-z0-9]+ lowercase, case-insensitive, niente smoothing.
' EXPECT: bleu_ident:  1|bleu_zero:  0|bleu_bp:  0.606531|rouge_ident:  1|rouge_413:  0.307692|rouge_45:  0.8
IMPORT TORCH
SUB Main()
  PRINT "bleu_ident: "; ROUND(TORCH.BLEU("the cat sat on the mat", "the cat sat on the mat"), 6)
  PRINT "bleu_zero: "; ROUND(TORCH.BLEU("the cat is on the mat", "the the the the the the the"), 6)
  PRINT "bleu_bp: "; ROUND(TORCH.BLEU("a b c d e f", "a b c d"), 6)
  PRINT "rouge_ident: "; ROUND(TORCH.ROUGE("the cat sat on the mat", "the cat sat on the mat"), 6)
  PRINT "rouge_413: "; ROUND(TORCH.ROUGE("the cat is on the mat", "the the the the the the the"), 6)
  PRINT "rouge_45: "; ROUND(TORCH.ROUGE("a b c d e f", "a b c d"), 6)
END SUB
