' MiniTest: registro delle opzioni — scrivere due chiavi di console e rileggerle.
' Comando: AI SET OPTION (scrittura nel registro); funzione: AI.OPTION(nome) (lettura).
' La terza chiave non viene impostata: AI.OPTION restituisce stringa vuota.
' EXPECT: GMB_CONSOLE_SIZE = 80,25|GMB_CONSOLE_FONT = Monaco,14|GMB_GFX_SCALE    = []
IMPORT AI WITH ALIAS
SUB Main()
  AI SET OPTION "GMB_CONSOLE_SIZE", "80,25"
  AI SET OPTION "GMB_CONSOLE_FONT", "Monaco,14"
  PRINT "GMB_CONSOLE_SIZE = "; AI.OPTION("GMB_CONSOLE_SIZE")
  PRINT "GMB_CONSOLE_FONT = "; AI.OPTION("GMB_CONSOLE_FONT")
  PRINT "GMB_GFX_SCALE    = ["; AI.OPTION("GMB_GFX_SCALE"); "]"
END SUB
