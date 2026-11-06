' Registro delle opzioni: scrivere due chiavi di console e rileggerle come valori.
' Eseguibile su tutte e quattro le gambe (IL, PCodeVM, VM/C, C99Native).
IMPORT AI WITH ALIAS

SUB Main()
  ' 1. scrittura nel registro: queste due chiavi vincono sulle impostazioni
  '    consoleConfiguration (capitolo VI.38, §7bis.1).
  AI SET OPTION "GMB_CONSOLE_SIZE", "80,25"
  AI SET OPTION "GMB_CONSOLE_FONT", "Monaco,14"

  ' 2. lettura delle stesse chiavi come valori.
  PRINT "GMB_CONSOLE_SIZE = "; AI.OPTION("GMB_CONSOLE_SIZE")
  PRINT "GMB_CONSOLE_FONT = "; AI.OPTION("GMB_CONSOLE_FONT")
  PRINT "GMB_GFX_SCALE    = ["; AI.OPTION("GMB_GFX_SCALE"); "]"

  ' 3. le due chiavi scritte compaiono anche in AI LIST OPTIONS con questo valore;
  '    GMB_GFX_SCALE resta (vuoto) perche' nessuno l'ha impostata.
END SUB
