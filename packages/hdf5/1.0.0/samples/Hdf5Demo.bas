REM FILE: Hdf5Demo.bas — pacchetto `hdf5` (namespace HDF5): esempio didattico completo.
REM Scenario: archiviare una serie di barre OHLC + volumi e i metadati di mercato in un file
REM HDF5, comprimere i rendimenti, rileggere tutto e verificare contenuto, dimensioni e attributi.
REM Il programma usa TUTTE le 19 funzioni del namespace HDF5. Output deterministico, identico
REM sui 4 backend (IL, PCode, VM/C, C99 nativo).
REQUIRE "hdf5"
IMPORT HDF5

SUB Main()
  WK$ = PROJECTFOLDER() + "/work"
  IF IO.DirExists(WK$) = 0 THEN MKDIR WK$
  P$ = WK$ + "/barre.h5"

  ' ── 1. creazione e struttura ───────────────────────────────────────────────────────
  ' HDF5 non crea i gruppi intermedi: "barre" deve esistere prima di "barre/ohlc".
  fh = HDF5.H5CREATE(P$)
  PRINT "creato="; (fh <> 0);
  PRINT "|gruppo_barre="; (HDF5.H5MAKEGROUP(fh, "barre") = 0);

  ' ── 2. dataset OHLC: 4 chiusure x 4 campi (open, high, low, close) ────────────────
  DIM dims(1) AS LONG
  dims(0) = 4 : dims(1) = 4
  DIM ohlc(15) AS DOUBLE
  FOR i = 0 TO 3
    ohlc(i*4+0) = 100 + i      ' open
    ohlc(i*4+1) = 102 + i      ' high
    ohlc(i*4+2) = 99 + i       ' low
    ohlc(i*4+3) = 101 + i      ' close
  NEXT i
  PRINT "|ohlc="; (HDF5.H5WRITEF64(fh, "barre/ohlc", 2, dims, ohlc) = 0);

  ' volumi interi a 64 bit (I64): il tipo viene preservato nel file
  DIM d1(0) AS LONG
  d1(0) = 4
  DIM vol(3) AS LONG
  FOR i = 0 TO 3 : vol(i) = 1000 * (i + 1) : NEXT i
  PRINT "|vol="; (HDF5.H5WRITEI64(fh, "barre/volumi", 1, d1, vol) = 0);

  ' rendimenti giornalieri compressi (deflate 4 + shuffle): utile su serie lunghe
  DIM rdim(0) AS LONG
  rdim(0) = 4
  DIM ret(3) AS DOUBLE
  FOR i = 0 TO 3 : ret(i) = (i - 1) * 0.01 : NEXT i
  PRINT "|ret="; (HDF5.H5WRITEF64C(fh, "barre/rendimenti", 1, rdim, ret, 4) = 0);

  ' ── 3. metadati: gruppo dedicato + attributi sull'oggetto ─────────────────────────
  PRINT "|gruppo_meta="; (HDF5.H5MAKEGROUP(fh, "meta") = 0);
  PRINT "|simbolo="; (HDF5.H5WRITEATTRSTR(fh, "barre/ohlc", "symbol", "AAPL") = 0);
  PRINT "|tick="; (HDF5.H5WRITEATTRF64(fh, "barre/ohlc", "tick", 0.01) = 0);

  ' ── 4. enumerazione degli oggetti: quanti, che tipo, che nome ─────────────────────
  n = HDF5.H5COUNT(fh)
  PRINT "|oggetti="; n;
  FOR k = 0 TO n - 1
    PRINT "|"; k; ":"; HDF5.H5KIND(fh, k); ":"; HDF5.H5NAME(fh, k);
  NEXT k
  CALL HDF5.H5CLOSE(fh)

  ' ── 5. riapertura e lettura: dimensioni, valori, tipi ─────────────────────────────
  fh = HDF5.H5OPEN(P$)
  PRINT "|riaperto="; (fh <> 0);
  PRINT "|n_ohlc="; HDF5.H5SIZE(fh, "barre/ohlc");

  DIM back(15) AS DOUBLE
  DIM rdims(7) AS LONG
  DIM nelem(0) AS LONG
  PRINT "|lettura="; (HDF5.H5READF64(fh, "barre/ohlc", back, rdims, nelem) = 0); ":";
  PRINT nelem(0); ":"; rdims(0); "x"; rdims(1); ":";
  PRINT back(3); ","; back(15 - 1);

  ' I64 tipizzato: il volume dell'ultimo giorno
  DIM bvol(3) AS LONG
  PRINT "|vol_ok="; (HDF5.H5READI64(fh, "barre/volumi", bvol, rdims, nelem) = 0); ":";
  PRINT bvol(3);

  ' lettura type-agnostica: qualunque dataset numerico diventa double
  DIM dvol(3) AS DOUBLE
  PRINT "|vol_dbl="; (HDF5.H5READDBL(fh, "barre/volumi", dvol, rdims, nelem) = 0); ":";
  PRINT dvol(0);

  ' attributi (stringa) e versione della libhdf5 in uso (deve essere la stessa sui 4 backend)
  PRINT "|simbolo="; HDF5.H5READATTRSTR(fh, "barre/ohlc", "symbol");
  ' H5READSTR serve ai file scritti da altri tool (dataset di STRINGHE): su un dataset
  ' numerico torna stringa vuota, perché questo wrapper scrive stringhe solo come attributi.
  PRINT "|str_numerico_vuota="; (LEN(HDF5.H5READSTR(fh, "barre/ohlc")) = 0);
  PRINT "|hdf5="; HDF5.H5VERSION()
  CALL HDF5.H5CLOSE(fh)
  PRINT ""
END SUB
