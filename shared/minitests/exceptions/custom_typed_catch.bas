' MiniTest EXCEPTIONS — eccezioni utente (`THROW NEW <Classe>`) e CATCH tipizzato su 4 gambe.
'
' Copre: classe esatta, EREDITARIETÀ (`CATCH e AS Base` cattura la derivata), `AS ERR` come
' catch-all (numero/descrizione), `THROW <codice>` numerico, e la RIPROPAGAZIONE quando nessuna
' clausola corrisponde (il TRY interno rilancia a quello esterno), filtro `WHEN` su ERR
' (e.Number) e su un campo della CLASSE lanciata.
' Prima: C99Native non compilava (`(int)(gmb_val_t)`), PCode/VM-C eseguivano sempre la PRIMA
' clausola (silenzioso), solo IL era corretto.
'
' EXPECT: C:boom: 7|B:boom2|E: 256|N: 999|W: 53|F:f|O:inner
CLASS ErrBase
  PUBLIC DIM Msg AS STRING
  PUBLIC SUB NEW(m AS STRING)
    SELF.Msg = m
  END SUB
END CLASS

CLASS ErrChild INHERITS ErrBase
  PUBLIC DIM Code AS LONG
  PUBLIC SUB NEW(m AS STRING, c AS LONG)
    BASE.NEW(m)
    SELF.Code = c
  END SUB
END CLASS

SUB Main()
  ' (1) classe ESATTA + lettura dei campi dell'istanza catturata
  TRY
    THROW NEW ErrChild("boom", 7)
  CATCH e AS ErrChild
    PRINT "C:"; e.Msg; ":"; e.Code
  CATCH e AS ErrBase
    PRINT "BAD-base"
  END TRY

  ' (2) EREDITARIETÀ: la clausola sulla BASE cattura la DERIVATA
  TRY
    THROW NEW ErrChild("boom2", 8)
  CATCH e AS ErrBase
    PRINT "B:"; e.Msg
  END TRY

  ' (3) AS ERR = catch-all: cattura anche un'istanza di classe (numero 256 = UserDefined)
  TRY
    THROW NEW ErrBase("x")
  CATCH e AS ErrChild
    PRINT "BAD-child"
  CATCH e AS ERR
    PRINT "E:"; e.Number
  END TRY

  ' (4) THROW numerico: la clausola di classe non corrisponde, ERR sì
  TRY
    THROW 999
  CATCH e AS ErrBase
    PRINT "BAD-num"
  CATCH e AS ERR
    PRINT "N:"; e.Number
  END TRY

  ' (5) WHEN su ERR: vince la clausola con il filtro soddisfatto (non la prima)
  TRY
    ERROR 53, ""
  CATCH e AS ERR WHEN e.Number = 11
    PRINT "BAD-11"
  CATCH e AS ERR WHEN e.Number = 53
    PRINT "W:"; e.Number
  CATCH e AS ERR
    PRINT "BAD-fallback"
  END TRY

  ' (6) WHEN su CLASSE: il filtro legge un campo dell'istanza lanciata
  TRY
    THROW NEW ErrChild("f", 9)
  CATCH e AS ErrChild WHEN e.Code = 8
    PRINT "BAD-code"
  CATCH e AS ErrChild WHEN e.Code = 9
    PRINT "F:"; e.Msg
  END TRY

  ' (7) nessuna clausola dell'inner: RIPROPAGA al TRY esterno
  TRY
    TRY
      THROW NEW ErrBase("inner")
    CATCH e AS ErrChild
      PRINT "BAD-inner"
    END TRY
  CATCH e AS ErrBase
    PRINT "O:"; e.Msg
  END TRY
END SUB
