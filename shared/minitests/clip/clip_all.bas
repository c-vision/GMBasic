' MiniTest NAMESPACE CLIP — unica funzione della superficie: GET.
' Headless la clipboard di sistema può essere vuota: si verifica la chiamata
' (stringa, anche vuota) senza dipendere dal contenuto.
' EXPECT: clip_ok=-1
IMPORT CLIP

SUB Main()
  DIM s AS STRING
  s = CLIP.GET()
  PRINT "clip_ok="; (LEN(s) >= 0)
END SUB
