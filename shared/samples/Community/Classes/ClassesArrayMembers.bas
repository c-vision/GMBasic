' FILE: Classes/ClassesArrayMembers.bas
' =================================================================================================
'   GUIDA: ARRAY COME MEMBRO di una CLASSE — scalari (INTEGER/DOUBLE/STRING), UDT, e array UDT
'          GLOBALE. Tutte le combinazioni: NUDO (`arr(i)`) e qualificato (`SELF.arr(i)`).
' =================================================================================================
'
' Un campo di una CLASS può essere un array: `PUBLIC DIM nome(N) AS tipo` (N = limite superiore,
' elementi 0..N). L'array è PER-ISTANZA (ogni oggetto ha il proprio buffer), allocato alla
' creazione. Si indicizza NUDO (`nome(i)`) o qualificato (`SELF.nome(i)`), in lettura e scrittura.
'
' Tipi di elemento:
'   - scalari: INTEGER, DOUBLE, STRING (un array STRING è una lista di stringhe).
'   - UDT (TYPE): l'elemento è una struct; si accede al campo con `arr(i).campo`
'     (e `SELF.arr(i).campo`).
' Un array di UDT a livello MODULO è un array globale: `DIM g(n) AS Tipo` → `g(i).campo`.

TYPE Holding
  shares AS INTEGER
  cost AS DOUBLE
END TYPE

CLASS Portfolio
  PUBLIC DIM tickers(4) AS STRING
  PUBLIC DIM prices(4) AS DOUBLE
  PUBLIC DIM holdings(4) AS Holding
  PUBLIC DIM count AS INTEGER

  PUBLIC SUB NEW()
    count = 0
  END SUB

  ' Aggiunge un titolo: scavla STRING e DOUBLE negli array-membro (scrittura NUDa).
  SUB AddTicker(t AS STRING, p AS DOUBLE)
    tickers(count) = t
    prices(count) = p
    count = count + 1
  END SUB

  ' Imposta una posizione (UDT): scrittura bare + qualificata (SELF.) del campo dell'elemento.
  SUB SetHolding(i AS INTEGER, sh AS INTEGER, c AS DOUBLE)
    holdings(i).shares = sh
    SELF.holdings(i).cost = c
  END SUB

  ' Valore totale: legge DOUBLE e il campo dell'elemento UDT (bare).
  FUNCTION TotalValue() AS DOUBLE
    DIM i AS INTEGER
    DIM t AS DOUBLE
    FOR i = 0 TO count - 1
      t = t + prices(i) * holdings(i).shares
    NEXT i
    TotalValue = t
  END FUNCTION

  ' Elenco titoli: concatena gli elementi STRING (bare).
  FUNCTION TickerList() AS STRING
    DIM i AS INTEGER
    DIM r AS STRING
    FOR i = 0 TO count - 1
      r = r + SELF.tickers(i) + " "
    NEXT i
    TickerList = r
  END FUNCTION
END CLASS

' Array di UDT a livello MODULO (globale): accesso `g(i).campo`.
DIM reservations(2) AS Holding

' Array-membro MULTI-DIMENSIONALE: `PUBLIC DIM nome(N,M) AS tipo` (indici 0..N, 0..M).
' Si indicizza con piu' indici: `nome(i,j)` (nudo) o `SELF.nome(i,j)`; per un elemento UDT,
' `nome(i,j).campo`. Vale per scalari e UDT, dentro e fuori la classe.
CLASS Grid
  PUBLIC DIM cells(2, 2) AS INTEGER
  PUBLIC DIM slots(1, 1) AS Holding

  PUBLIC SUB NEW()
  END SUB

  FUNCTION Total() AS INTEGER
    DIM i AS INTEGER
    DIM j AS INTEGER
    DIM s AS INTEGER
    FOR i = 0 TO 2
      FOR j = 0 TO 2
        cells(i, j) = i * 3 + j
      NEXT j
    NEXT i
    s = 0
    FOR i = 0 TO 2
      FOR j = 0 TO 2
        s = s + cells(i, j)
      NEXT j
    NEXT i
    Total = s
  END FUNCTION

  ' Campo di un elemento UDT 2-D: `slots(i,j).campo`.
  SUB SetSlot(i AS INTEGER, j AS INTEGER, sh AS INTEGER)
    slots(i, j).shares = sh
  END SUB

  FUNCTION SlotShares(i AS INTEGER, j AS INTEGER) AS INTEGER
    SlotShares = slots(i, j).shares
  END FUNCTION
END CLASS

SUB Main()
  DIM p AS Portfolio = NEW Portfolio()
  p.AddTicker("AAPL", 150.5)
  p.AddTicker("MSFT", 300.0)
  p.SetHolding(0, 10, 100.0)
  p.SetHolding(1, 5, 200.0)

  reservations(0).shares = 7
  reservations(0).cost = 12.5
  reservations(1).shares = 3

  DIM g AS Grid = NEW Grid()
  g.SetSlot(1, 1, 9)

  PRINT "tickers="; p.TickerList()
  PRINT "value="; p.TotalValue()
  PRINT "count="; p.count
  PRINT "res0="; reservations(0).shares
  PRINT "res1="; reservations(1).shares
  PRINT "grid="; g.Total()
  PRINT "slots="; g.SlotShares(1, 1)
END SUB
