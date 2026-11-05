' TAG: unsupported-c99native
' EXPECT: creato=-1 |wr_f64=-1 |wr_i64=-1 |f64_su_i64=-1 |i64_su_f64=-1 |dbl_su_i64=-1 : 0 |manca_rd=-1 |manca_size=-1 |nome_99=-1 |kind_99= 2 |h0_count=-1 |h0_size=-1 |h0_rd=-1 |h0_close= 0 |open_manca=-1
' HDF5 (pacchetto gmblib-hdf5) — CODICI DI ERRORE e semantica dei parametri.
' Verifica che ogni caso limite ritorni il codice documentato invece di leggere/scrivere
' memoria casuale: handle nullo, dataset assente, tipo inatteso, indice fuori range.
REQUIRE "hdf5"
IMPORT HDF5

WK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WK$) = 0 THEN MKDIR WK$
P$ = WK$ + "/errori.h5"

SUB Main()
  DIM d1(0) AS LONG
  DIM v1(3) AS DOUBLE
  DIM i1(3) AS LONG
  DIM dims(7) AS LONG
  DIM nelem(0) AS LONG
  d1(0) = 4
  FOR i = 0 TO 3 : v1(i) = i + 0.5 : i1(i) = i * 2 : NEXT i

  fh = HDF5.H5CREATE(P$)
  PRINT "creato="; (fh <> 0);
  PRINT "|wr_f64="; (HDF5.H5WRITEF64(fh, "serie", 1, d1, v1) = 0);
  PRINT "|wr_i64="; (HDF5.H5WRITEI64(fh, "conti", 1, d1, i1) = 0);

  ' ── tipo inatteso: -2 ────────────────────────────────────────────────────────────
  DIM rd(7) AS DOUBLE
  DIM ri(7) AS LONG
  PRINT "|f64_su_i64="; HDF5.H5READF64(fh, "conti", rd, dims, nelem);
  PRINT "|i64_su_f64="; HDF5.H5READI64(fh, "serie", ri, dims, nelem);
  ' la lettura type-agnostica invece funziona su entrambi
  PRINT "|dbl_su_i64="; HDF5.H5READDBL(fh, "conti", rd, dims, nelem); ":"; rd(3);

  ' ── dataset assente: -1 (lettura e dimensione) ───────────────────────────────────
  PRINT "|manca_rd="; HDF5.H5READF64(fh, "inesistente", rd, dims, nelem);
  PRINT "|manca_size="; HDF5.H5SIZE(fh, "inesistente");

  ' ── indice fuori range nell'enumerazione: nome vuoto, tipo 2 (ALTRO) ─────────────
  PRINT "|nome_99="; (LEN(HDF5.H5NAME(fh, 99)) = 0);
  PRINT "|kind_99="; HDF5.H5KIND(fh, 99);

  ' ── handle nullo / inesistente ──────────────────────────────────────────────────
  PRINT "|h0_count="; HDF5.H5COUNT(0);
  PRINT "|h0_size="; HDF5.H5SIZE(0, "serie");
  PRINT "|h0_rd="; HDF5.H5READF64(0, "serie", rd, dims, nelem);
  PRINT "|h0_close="; HDF5.H5CLOSE(0);
  PRINT "|open_manca="; (HDF5.H5OPEN(WK$ + "/non_esiste.h5") = 0);

  CALL HDF5.H5CLOSE(fh)
  PRINT ""
END SUB
