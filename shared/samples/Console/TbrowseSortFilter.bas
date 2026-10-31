' TbrowseSortFilter.bas — TBROWSE: ordinamento (colonna, asc/desc) e filtro testo (§12.4).
' Il motore sort/filter è nel runtime TBROWSE (gmb_browse_sort / gmb_browse_set_filter);
' qui si pilota dalla superficie via CONTROL SET/GET.
IMPORT CONTROLS
SCREEN 0

SUB Main()
  DIM hd(2) AS STRING
  hd(0) = "Simbolo" : hd(1) = "Prezzo" : hd(2) = "Qta"
  DIM dt(3, 2) AS STRING
  dt(0, 0) = "MSFT" : dt(0, 1) = "250" : dt(0, 2) = "10"
  dt(1, 0) = "AAPL" : dt(1, 1) = "100" : dt(1, 2) = "20"
  dt(2, 0) = "NVDA" : dt(2, 1) = "500" : dt(2, 2) = "5"
  DIM sel AS INTEGER
  TBROWSE 2, 2, 40, 8, hd, dt INTO sel NAME "book"

  ' Ordina per Prezzo (colonna 1) crescente.
  CONTROL SET "book", "sort", 1, ""
  CONTROL SET "book", "sortdir", 0, ""
  DIM sc AS INTEGER
  DIM sd AS INTEGER
  CONTROL GET "book", "sort" INTO sc
  CONTROL GET "book", "sortdir" INTO sd
  PRINT "ordinato per colonna "; sc; " dir="; sd

  ' Filtro: mostra solo le righe che contengono "A".
  CONTROL SET "book", "filter", 0, "A"
  DIM f AS STRING
  CONTROL GETS "book", "filter" INTO f
  PRINT "filtro="; f

  ' Azzera il filtro.
  CONTROL SET "book", "filter", 0, ""
  PRINT "tbrowse-sortfilter-demo-ok"
END SUB
