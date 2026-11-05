REM FILE: Hdf5Demo.bas — pacchetto `hdf5` (namespace HDF5): esempio didattico completo.
REM Scenario: archiviare una serie di barre OHLC + i metadati in un file HDF5, comprimerla,
REM rileggerla e verificarne contenuto, dimensioni e attributi.
REQUIRE "hdf5"
IMPORT HDF5

WK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WK$) = 0 THEN MKDIR WK$
P$ = WK$ + "/barre.h5"

' ── 1. creazione e scrittura ─────────────────────────────────────────────────────
fh = HDF5.H5CREATE(P$)
PRINT "creato="; (fh <> 0);

' HDF5 non crea i gruppi intermedi: il gruppo "barre" va creato prima dei dataset annidati.
PRINT "|gruppo_barre="; (HDF5.H5MAKEGROUP(fh, "barre") = 0);

' chiusura del giorno (n=4) x 4 campi (open,high,low,close) = dataset 4x4
DIM dims(1) AS LONG
dims(0) = 4 : dims(1) = 4
DIM ohlc(15) AS DOUBLE
FOR i = 0 TO 3
  ohlc(i*4+0) = 100 + i
  ohlc(i*4+1) = 102 + i
  ohlc(i*4+2) = 99 + i
  ohlc(i*4+3) = 101 + i
NEXT i
PRINT "|ohlc="; (HDF5.H5WRITEF64(fh, "barre/ohlc", 2, dims, ohlc) = 0);

' volumi interi, compressi (deflate 4): spesso si comprimono bene
DIM d1(0) AS LONG
d1(0) = 4
DIM vol(3) AS LONG
FOR i = 0 TO 3 : vol(i) = 1000 * (i + 1) : NEXT i
DIM cdims(0) AS LONG
cdims(0) = 4
DIM cvol(3) AS LONG
FOR i = 0 TO 3 : cvol(i) = vol(i) : NEXT i
PRINT "|vol="; (HDF5.H5WRITEI64(fh, "barre/volumi", 1, d1, vol) = 0);

' gruppo di metadati + attributi
PRINT "|gruppo="; (HDF5.H5MAKEGROUP(fh, "meta") = 0);
PRINT "|simbolo="; (HDF5.H5WRITEATTRSTR(fh, "barre/ohlc", "symbol", "AAPL") = 0);
PRINT "|scala="; (HDF5.H5WRITEATTRF64(fh, "barre/ohlc", "tick", 0.01) = 0);
PRINT "|oggetti="; HDF5.H5COUNT(fh);
CALL HDF5.H5CLOSE(fh)

' ── 2. riapertura, lettura e verifiche ───────────────────────────────────────────
fh = HDF5.H5OPEN(P$)
PRINT "|n_ohlc="; HDF5.H5SIZE(fh, "barre/ohlc");

DIM back(15) AS DOUBLE
DIM rdims(7) AS LONG
DIM nelem(0) AS LONG
PRINT "|lettura="; (HDF5.H5READF64(fh, "barre/ohlc", back, rdims, nelem) = 0); ":";
PRINT nelem(0); ":"; rdims(0); "x"; rdims(1); ":";
PRINT back(3); ","; back(15-1);

' i volumi sono interi: lettura I64 (e, in alternativa, type-agnostica in double)
DIM bvol(3) AS LONG
PRINT "|vol_ok="; (HDF5.H5READI64(fh, "barre/volumi", bvol, rdims, nelem) = 0); ":"; bvol(3);
DIM dvol(3) AS DOUBLE
PRINT "|vol_dbl="; (HDF5.H5READDBL(fh, "barre/volumi", dvol, rdims, nelem) = 0); ":"; dvol(0);

' metadati
PRINT "|simbolo="; HDF5.H5READATTRSTR(fh, "barre/ohlc", "symbol");
PRINT "|hdf5="; (LEN(HDF5.H5VERSION()) > 0)
CALL HDF5.H5CLOSE(fh)
PRINT ""
