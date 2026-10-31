' ============================================================================
' TorchCommands.bas — TORCH: comandi legacy a spazio (nessun valore di ritorno)
' ============================================================================
' I comandi TORCH completano le funzioni qualificate TORCH.XXX(...):
'   TORCH INFO                          -> stampa stato (device, disponibili, lib path, loaded)
'   TORCH LOAD LIB                      -> carica la lib ML (idempotente)
'   TORCH UNLOAD LIB                    -> scarica la lib ML
'   TORCH SET DEVICE "auto"|"cpu"|"cuda"|"rocm"|"mlx"  -> imposta il device attivo
'   TORCH SET LIB PATH "dir"            -> override della directory delle librerie
' Regola sintattica: i comandi NON usano le parentesi e NON ritornano valori.
' ============================================================================
IMPORT TORCH

SUB Main()
  PRINT "== TORCH: comandi legacy =="
  TORCH INFO

  TORCH LOAD LIB
  PRINT "loaded dopo LOAD        ="; TORCH.LOADED()

  TORCH SET DEVICE "cpu"
  PRINT "device dopo SET DEVICE  ="; TORCH.DEVICE()

  TORCH UNLOAD LIB
  PRINT "loaded dopo UNLOAD      ="; TORCH.LOADED()

  TORCH LOAD LIB
  PRINT "loaded dopo 2o LOAD     ="; TORCH.LOADED()

  ' verifica che le funzioni TORCH.* funzionino ancora
  z = TORCH.ZEROS(2, 2)
  PRINT "zeros sum ="; TORCH.SUM(z)

  PRINT "done"
END SUB
