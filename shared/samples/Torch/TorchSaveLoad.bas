' TORCH — persistenza dei pesi: MODELSAVE scrive un tensore flat, MODELLOAD lo
' ricarica in un modello creato con la stessa architettura.
'
' Scenario: si addestra un piccolo modello lineare, si salva il checkpoint, si crea
' un secondo modello (inizializzazione diversa) e si verifica che, dopo MODELLOAD,
' il suo forward coincida con quello del modello originale. Gli output stampati sono
' solo gli esiti (0 = ok) e i verdetti testuali: i valori numerici del forward
' dipendono dall'inizializzazione e non sono confrontabili tra esecuzioni.
IMPORT TORCH

SUB Main()
  DIM cfg AS STRING = "{""in"":2,""out"":1}"
  DIM p AS STRING = "/tmp/gmb_torch_saveload.pt"
  ON ERROR RESUME NEXT
  KILL p
  ON ERROR GOTO 0

  ' ── modello A: addestrato su un esempio ───────────────────────────────────
  DIM m1 AS LONG = TORCH.MODELCREATE("linear", cfg)
  DIM x AS LONG = TORCH.ZEROS(1, 2)
  TORCH.SET(x, 0, 0, 0.3) : TORCH.SET(x, 0, 1, -0.7)
  DIM y AS LONG = TORCH.ZEROS(1, 1)
  TORCH.SET(y, 0, 0, 1.25)
  DIM i AS LONG
  FOR i = 1 TO 30
    TORCH.MODELTRAINSTEP(m1, x, y, "sgd", "mse", 0.1)
  NEXT i
  PRINT "save="; TORCH.MODELSAVE(m1, p)

  ' ── modello B: stessa architettura, pesi iniziali diversi ─────────────────
  DIM m2 AS LONG = TORCH.MODELCREATE("linear", cfg)
  PRINT "load="; TORCH.MODELLOAD(m2, p)

  ' ── il forward dei due modelli deve coincidere dopo il load ───────────────
  DIM f1 AS LONG = TORCH.MODELFORWARD(m1, x)
  DIM f2 AS LONG = TORCH.MODELFORWARD(m2, x)
  DIM a1() = TORCH.TOARRAY(f1)
  DIM a2() = TORCH.TOARRAY(f2)
  IF ABS(a1(0) - a2(0)) < 0.0000001 THEN
    PRINT "roundtrip: ok"
  ELSE
    PRINT "roundtrip: ko"
  END IF

  ' ── un path inesistente e' un errore esplicito (esito diverso da 0) ───────
  DIM rc AS LONG = TORCH.MODELLOAD(m2, "/tmp/gmb_torch_saveload_non_esiste.pt")
  IF rc <> 0 THEN
    PRINT "badfile: ok"
  ELSE
    PRINT "badfile: ko"
  END IF

  PRINT "free="; TORCH.MODELFREE(m1); TORCH.MODELFREE(m2)
  TORCH.FREE(x) : TORCH.FREE(y) : TORCH.FREE(f1) : TORCH.FREE(f2)
  ON ERROR RESUME NEXT
  KILL p
  ON ERROR GOTO 0
END SUB
