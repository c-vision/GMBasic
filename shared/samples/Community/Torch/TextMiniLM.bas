' ============================================================================
' TextMiniLM.bas — addestra un piccolo transformer (carattere-level) su un testo
' con TORCH e mostra la loss che scende. Uso CONCRETO del training torch attuale:
' un mini-LM embedded su un dominio piccolo (addestrato da zero, CPU).
'   - vocabolario = lettere minuscole + spazio (id: spazio=1, a=2..z=27)
'   - windows [1,S-1] token → predice il prossimo carattere (teacher forcing)
'   - loss crossentropy + adamw
' Nota: il campionamento greedy è dimostrato nativamente (TorchDistill/TorchAll usano
' MODELFORWARD); campionare token da logits in BASIC richiede un helper nativo
' (es. TORCH.ARGMAX) — non ancora esposto. Qui il gate è la loss che scende.
' ============================================================================
IMPORT TORCH

CONST S% = 8
CONST CHARSET$ = " abcdefghijklmnopqrstuvwxyz"

FUNCTION CharId(c AS STRING) AS LONG
  IF c = " " THEN CharId = 1 ELSE CharId = ASC(c) - 96
END FUNCTION

SUB Main()
  DIM corpus AS STRING
  corpus = "the quick brown fox jumps over the lazy dog and the dog runs fast but the fox is quick again"

  DIM L AS LONG = LEN(corpus)
  IF L < S% + 8 THEN PRINT "corpus troppo corto": RETURN
  DIM NW AS LONG = L - S%
  IF NW > 40 THEN NW = 40

  DIM cfg AS STRING = "{""vocab"":28,""d_model"":32,""n_layers"":2,""n_heads"":2,""block_size"":16}"
  DIM m AS LONG = TORCH.MODELCREATE("transformer", cfg)

  DIM xs(6) AS DOUBLE
  DIM ys(6) AS DOUBLE
  DIM xh AS LONG
  DIM yh AS LONG
  DIM ep AS LONG, wnd AS LONG, i AS LONG

  PRINT "corpus len="; L; " windows="; NW; " vocab=28"
  FOR ep = 1 TO 60
    FOR wnd = 0 TO NW - 1
      FOR i = 0 TO S% - 2
        xs(i) = CharId(MID$(corpus, wnd + i + 1, 1))
        ys(i) = CharId(MID$(corpus, wnd + i + 2, 1))
      NEXT i
      xh = TORCH.RESHAPE(TORCH.FROM(xs), 1, S% - 1)
      yh = TORCH.RESHAPE(TORCH.FROM(ys), 1, S% - 1)
      TORCH.MODELTRAINSTEP(m, xh, yh, "adamw", "crossentropy", 0.01)
    NEXT wnd
    IF ep MOD 20 = 0 THEN PRINT "epoch "; ep; " loss="; TORCH.MODELLOSS(m, xh, yh)
  NEXT ep
  PRINT "LOSS end ="; TORCH.MODELLOSS(m, xh, yh)
  TORCH.MODELFREE(m)
  PRINT "=== TextMiniLM: done (loss bassa = il modello ha appreso la sequenza di caratteri) ==="
END SUB
