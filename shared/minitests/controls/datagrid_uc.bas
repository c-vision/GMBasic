' Minitest DataGrid UserControl (§12.4) — grid avanzato personalizzabile, 4 backend.
' EXPECT: datagrid-ok
IMPORT CONTROLS
IMPORT CustomControls

SUB Main()
  DIM g AS DataGrid = NEW DataGrid()
  CONTROL ADD g AT 1, 1 SIZE 46, 12
  g.Configure(3, 3)
  g.SetColumn(0, "Sym", 6, 0, 0, 0)
  g.SetColumn(1, "Val", 6, 2, 2, 1)
  g.SetColumn(2, "Qty", 5, 2, 1, 0)
  g.SetColumnColors(1, 16776960, -1)
  g.SetCell(0, 0, "b") : g.SetCell(0, 1, "3") : g.SetCell(0, 2, "30")
  g.SetCell(1, 0, "a") : g.SetCell(1, 1, "1") : g.SetCell(1, 2, "10")
  g.SetCell(2, 0, "c") : g.SetCell(2, 1, "2") : g.SetCell(2, 2, "20")
  g.SortBy(1, 1)
  DIM ok AS INTEGER
  ok = 0
  IF g.GetCell(0, 0) = "b" THEN ok = 1
  DIM sc AS INTEGER
  sc = g.SortCol()
  g.SetFilterText("a")
  DIM fv AS INTEGER
  fv = g.ViewCount()
  g.SetFilterText("")
  g.MousePress(2, 3)
  DIM cr AS INTEGER
  cr = g.CurRow()
  g.SetColumnColors(1, 16776960, -1)
  CONTROL REMOVE g
  IF ok = 1 AND sc = 1 AND fv = 1 AND cr = 2 THEN
    PRINT "datagrid-ok"
  ELSE
    PRINT "datagrid-no "; ok; "|"; sc; "|"; fv; "|"; cr
  END IF
END SUB
