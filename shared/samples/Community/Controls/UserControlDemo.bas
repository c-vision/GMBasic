' UserControlDemo.bas — controlli utente (stile VB6) in GMBasic.
' Una classe diventa un controllo con `CONTROL ADD istanza, x, y, w, h`.
' (P0: la classe è registrata come controllo; il metodo Paint produce il disegno.)
IMPORT CONTROLS

' Classe utente: eredita il modello Control e ne ridefinisce il rendering.
CLASS Sparkline
  PRIVATE DIM mPhase AS DOUBLE

  SUB Paint(g AS ANY)
    ' Contesto grafico del controllo (coordinate locali 0..Width / 0..Height).
    ' API: g.Clear, g.Line, g.Rect, g.FillRect, g.Circle, g.Text, g.Plot, g.SetColor.
  END SUB

  SUB Tick(d AS DOUBLE)
    mPhase = mPhase + d
  END SUB
END CLASS

SUB Main()
  DIM spark AS Sparkline = NEW Sparkline()
  CONTROL ADD spark AT 20, 20 SIZE 240, 80

  ' ... il programma aggiorna lo stato; il frame richiama Paint.
  DIM i AS INTEGER
  FOR i = 1 TO 10
    spark.Tick(0.5)
  NEXT i

  CONTROL REMOVE spark
  PRINT "OK-usercontrol-demo"
END SUB
