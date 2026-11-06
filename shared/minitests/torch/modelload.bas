' MiniTest: TORCH.MODELLOAD — ricarica i pesi salvati da MODELSAVE (parita' 4 backend).
' Un modello lineare viene addestrato, salvato, e ricaricato in un secondo modello
' creato con la stessa architettura: il forward sul nuovo modello deve coincidere
' ESATTAMENTE con quello del modello originale (i pesi sono copiati, non ricalcolati).
' EXPECT: prima: diverso|save= 0  load= 0|after=0.000000|bad: 1
IMPORT TORCH
IMPORT IO

SUB Main()
  DIM cfg AS STRING = "{""in"":2,""out"":1}"
  DIM p AS STRING = "/tmp/gmb_modelload.pt"
  ON ERROR RESUME NEXT
  KILL p
  ON ERROR GOTO 0

  ' ── modello A: addestrato su due esempi (i pesi cambiano) ──────────────────
  DIM m1 AS LONG = TORCH.MODELCREATE("linear", cfg)
  DIM x(1) AS DOUBLE = {0.3, -0.7}
  DIM y(0) AS DOUBLE = {1.25}
  DIM t AS LONG = TORCH.RESHAPE(TORCH.FROM(x), 1, 2)
  DIM ty AS LONG = TORCH.RESHAPE(TORCH.FROM(y), 1, 1)
  DIM i AS LONG
  FOR i = 1 TO 30
    TORCH.MODELTRAINSTEP(m1, t, ty, "sgd", "mse", 0.1)
  NEXT i
  DIM o1 AS LONG = TORCH.MODELFORWARD(m1, t)
  DIM a1() = TORCH.TOARRAY(o1)

  ' ── modello B: stessa architettura, inizializzazione diversa ──────────────
  DIM m2 AS LONG = TORCH.MODELCREATE("linear", cfg)
  DIM o2a AS LONG = TORCH.MODELFORWARD(m2, t)
  DIM a2a() = TORCH.TOARRAY(o2a)
  IF ABS(a1(0) - a2a(0)) > 0 THEN
    PRINT "prima: diverso"
  ELSE
    PRINT "prima: uguale"
  END IF

  ' ── salva A e ricarica in B ───────────────────────────────────────────────
  DIM rc AS LONG = TORCH.MODELSAVE(m1, p)
  DIM rc2 AS LONG = TORCH.MODELLOAD(m2, p)
  PRINT "save="; rc; " load="; rc2

  DIM o2 AS LONG = TORCH.MODELFORWARD(m2, t)
  DIM a2() = TORCH.TOARRAY(o2)
  PRINT "after="; FORMAT(ABS(a1(0) - a2(0)), "#.000000")

  ' ── errore: file inesistente -> esito diverso da 0 ────────────────────────
  DIM rc3 AS LONG = TORCH.MODELLOAD(m2, "/tmp/gmb_modelload_non_esiste.pt")
  IF rc3 <> 0 THEN
    PRINT "bad: 1"
  ELSE
    PRINT "bad: 0"
  END IF

  TORCH.MODELFREE(m1) : TORCH.MODELFREE(m2)
  TORCH.MODELFREE(t) : TORCH.MODELFREE(ty)
  TORCH.MODELFREE(o1) : TORCH.MODELFREE(o2a) : TORCH.MODELFREE(o2)
  ON ERROR RESUME NEXT
  KILL p
  ON ERROR GOTO 0
END SUB
