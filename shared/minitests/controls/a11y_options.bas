' MiniTest: accessibilità runtime — i flag globali sono voci del registro opzioni.
' Scrittura: AI SET OPTION <nome>, <valore>   Lettura: AI.OPTION(<nome>).
' I quattro flag valgono su TUTTA l'interfaccia (percorso TEXT/TUI e GRAPHICS/PIXEL),
' non su un singolo controllo: alto contrasto, scala del testo, movimento ridotto,
' anello di focus sempre visibile. Un valore vuoto significa "default" (contrasto
' normale, scala 1.0, movimento e focus del tema).
' EXPECT: high_contrast = 1|high_contrast off = 0|text_scale = 1.5|reduce_motion = 1|focus_always_visible = 1|text_scale spento = []
IMPORT AI WITH ALIAS
SUB Main()
  AI SET OPTION "ui.high_contrast", "1"
  AI SET OPTION "ui.text_scale", "1.5"
  AI SET OPTION "ui.reduce_motion", "1"
  AI SET OPTION "ui.focus_always_visible", "1"
  PRINT "high_contrast = "; AI.OPTION("ui.high_contrast")
  AI SET OPTION "ui.high_contrast", "0"
  PRINT "high_contrast off = "; AI.OPTION("ui.high_contrast")
  PRINT "text_scale = "; AI.OPTION("ui.text_scale")
  PRINT "reduce_motion = "; AI.OPTION("ui.reduce_motion")
  PRINT "focus_always_visible = "; AI.OPTION("ui.focus_always_visible")
  AI SET OPTION "ui.text_scale", ""
  PRINT "text_scale spento = ["; AI.OPTION("ui.text_scale"); "]"
END SUB
