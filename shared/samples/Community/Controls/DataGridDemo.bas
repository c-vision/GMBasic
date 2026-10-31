' DataGridDemo.bas — demo d'applicazione del controllo (Controls/DataGrid.bas via IMPORT).
IMPORT CustomControls

SUB Main()
  DIM g AS DataGrid = NEW DataGrid()
  CONTROL ADD g AT 1, 1 SIZE 46, 12
  g.Configure(4, 5)
  g.SetColumn(0, "Simbolo", 10, 0, 0, 0)
  g.SetColumn(1, "Prezzo", 10, 2, 2, 1)
  g.SetColumn(2, "Var%", 8, 2, 2, 0)
  g.SetColumn(3, "Qta", 6, 2, 1, 1)
  ' Personalizzazione "senza sottoclasse": colori per colonna (config).
  g.SetColumnColors(1, 16776960, -1)   ' Prezzo: giallo
  g.SetColumnColors(2, 65280, -1)      ' Var%: verde
  g.SetCell(0, 0, "AAPL") : g.SetCell(0, 1, "190.50") : g.SetCell(0, 2, "1.2") : g.SetCell(0, 3, "120")
  g.SetCell(1, 0, "MSFT") : g.SetCell(1, 1, "410.00") : g.SetCell(1, 2, "-0.8") : g.SetCell(1, 3, "80")
  g.SetCell(2, 0, "NVDA") : g.SetCell(2, 1, "880.25") : g.SetCell(2, 2, "3.4") : g.SetCell(2, 3, "45")
  g.SetCell(3, 0, "TSLA") : g.SetCell(3, 1, "175.10") : g.SetCell(3, 2, "-2.1") : g.SetCell(3, 3, "210")
  g.SetCell(4, 0, "AMZN") : g.SetCell(4, 1, "182.75") : g.SetCell(4, 2, "0.5") : g.SetCell(4, 3, "60")
  g.SetOrigin(1, 1, 46, 12)

  ' ordina per Prezzo (col 1) decrescente
  g.SortBy(1, 1)
  PRINT "top dopo sort prezzo: "; g.GetCell(0, 0); " = "; g.GetCell(0, 1)
  PRINT "sort col="; g.SortCol(); " desc="; g.SortDesc()
  ' filtro: solo simboli con "S"
  g.SetFilterText("S")
  PRINT "righe filtrate (S): "; g.ViewCount()
  g.SetFilterText("")
  ' navigazione
  g.MoveCursor(2, 1)
  PRINT "curseur row="; g.CurRow(); " col="; g.CurCol()
  ' export CSV (prime 2 righe)
  PRINT "csv head:"; CHR(10); g.ExportCsv()
  PRINT "datagrid-ok"
END SUB
