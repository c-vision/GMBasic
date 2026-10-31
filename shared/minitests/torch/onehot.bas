' MiniTest: TORCH.ONEHOT (parita' 4 backend).
' EXPECT: 0  1  0
IMPORT TORCH
SUB Main()
  DIM o() = TORCH.ONEHOT(1, 3)
  PRINT o(0); o(1); o(2)
END SUB
