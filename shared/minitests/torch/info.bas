' MiniTest: TORCH info LOADED/DEVICE/AVAILABLE/DTYPE/DEVICEOF (parita' 4 backend).
' EXPECT: loaded: ok|device: ok|avail: ok|dtype: ok|devof: ok
IMPORT TORCH
SUB Main()
  ' Dopo un'operazione su tensore la libreria ML e' caricata (LOADED deterministico).
  t = TORCH.ONES(2, 2)
  IF TORCH.LOADED() THEN PRINT "loaded: ok" ELSE PRINT "loaded: fail"
  IF LEN(TORCH.DEVICE()) > 0 THEN PRINT "device: ok" ELSE PRINT "device: fail"
  IF LEN(TORCH.AVAILABLE()) > 0 THEN PRINT "avail: ok" ELSE PRINT "avail: fail"
  IF LEN(TORCH.DTYPE(t)) > 0 THEN PRINT "dtype: ok" ELSE PRINT "dtype: fail"
  IF LEN(TORCH.DEVICEOF(t)) > 0 THEN PRINT "devof: ok" ELSE PRINT "devof: fail"
  TORCH.FREE(t)
END SUB
