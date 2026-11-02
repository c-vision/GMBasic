' MiniTest EXCEPTIONS — semantica di FINALLY su 4 gambe.
'
' Copre: FINALLY eseguito DOPO il corpo del CATCH (non prima), FINALLY attraversato da
' un errore che sfugge a un TRY … FINALLY senza CATCH (e poi catturato dal TRY esterno).
' Prima: IL eseguiva il FINALLY PRIMA del CATCH; PCodeVM e VM/C non eseguivano affatto il
' FINALLY quando il TRY non aveva clausole CATCH.
'
' EXPECT: C1: 7|F1|F2|G: 42|done
SUB Main()
  ' (1) FINALLY dopo il corpo del CATCH
  TRY
    ERROR 7, ""
  CATCH e AS ERR
    PRINT "C1:"; e.Number
  FINALLY
    PRINT "F1"
  END TRY

  ' (2) TRY … FINALLY senza CATCH: il FINALLY esegue, poi l'errore prosegue al TRY esterno
  TRY
    TRY
      ERROR 42, "boom"
    FINALLY
      PRINT "F2"
    END TRY
    PRINT "BAD"
  CATCH e AS ERR
    PRINT "G:"; e.Number
  END TRY
  PRINT "done"
END SUB
