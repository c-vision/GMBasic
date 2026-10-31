' AiOptions.bas — Registro opzioni generico del namespace AI.
'
' Un solo meccanismo per tutti i parametri del motore: NON una keyword per-setting.
'   COMANDO  (scrittura, senza parentesi):  AI SET OPTION <nome>, <valore>
'   FUNZIONE (lettura, con parentesi):       AI.Option(<nome>)  -> stringa
'
' La voce viene registrata dall'host MLX (es. "moe_swap_dir" per lo streaming
' esperti MoE da disco). Un nome ignoto non e' un errore fatale: la scrittura
' non ha effetto e la lettura ritorna stringa vuota (comportamento verificato).
'
' Nota: richiede la libreria nativa AI (libgmb_ai_mlx) per la registrazione.
IMPORT AI WITH ALIAS

SUB Main()
  PRINT "== Registro opzioni AI =="

  ' 1) Scrittura di una voce (comando legacy: nessuna parentesi).
  AI SET OPTION "moe_swap_dir", "work/experts"

  ' 2) Lettura della stessa voce (funzione qualificata: parentesi obbligatorie).
  DIM dir AS STRING
  dir = AI.Option("moe_swap_dir")
  PRINT "moe_swap_dir = "; dir

  ' 3) Round-trip: il valore letto deve coincidere con quello scritto.
  IF dir = "work/experts" THEN
    PRINT "round-trip OK"
  ELSE
    PRINT "round-trip FALLITO"
  END IF

  ' 4) Voce ignota: nessuna eccezione, lettura vuota.
  DIM ignota AS STRING
  ignota = AI.Option("voce_inesistente")
  IF LEN(ignota) = 0 THEN
    PRINT "ignota=vuota (atteso)"
  ELSE
    PRINT "ignota="; ignota
  END IF

  ' 5) Aggiornamento della stessa voce (idempotente, sovrascrive).
  AI SET OPTION "moe_swap_dir", "work/experts2"
  PRINT "aggiornata = "; AI.Option("moe_swap_dir")
END SUB
