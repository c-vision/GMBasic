' TAG: unsupported-c99native
' EXPECT: create=-1 |wr_f64=-1 |wr_i64=-1 |wr_zip=-1 |grp=-1 |at_s=-1 |at_f=-1 |list= 4 : 0  0  0 |size= 6 |rd_f64=-1 : 6 : 2 x 3 : 0.5 , 5.5 |rd_i64=-1 : 30 |rd_zip=-1 : 5 |nome_serie= 1 |attr=eur|str_vuota=-1 |ver=-1
' HDF5 (pacchetto gmblib-hdf5): scrittura + rilettura di un dataset F64, un I64, un gruppo,
' attributi e listing. Nessuna fixture binaria: il file si crea e si legge nel test.
REQUIRE "hdf5"
IMPORT HDF5

SUB Main()

WK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WK$) = 0 THEN MKDIR WK$
P$ = WK$ + "/roundtrip.h5"

fh = HDF5.H5CREATE(P$)
PRINT "create="; (fh <> 0);

' dataset F64 2x3
DIM dims(1) AS LONG
dims(0)=2 : dims(1)=3
DIM vals(5) AS DOUBLE
FOR i = 0 TO 5 : vals(i) = i + 0.5 : NEXT i
PRINT "|wr_f64="; (HDF5.H5WRITEF64(fh, "serie", 2, dims, vals) = 0);

' dataset I64 1x4, compresso (deflate 4)
DIM d1(0) AS LONG
d1(0)=4
DIM idata(3) AS LONG
FOR i = 0 TO 3 : idata(i) = 10 * i : NEXT i
PRINT "|wr_i64="; (HDF5.H5WRITEI64(fh, "conteggi", 1, d1, idata) = 0);
DIM cdata(5) AS DOUBLE
FOR i = 0 TO 5 : cdata(i) = i : NEXT i
PRINT "|wr_zip="; (HDF5.H5WRITEF64C(fh, "compressa", 2, dims, cdata, 4) = 0);

' gruppo + attributi
PRINT "|grp="; (HDF5.H5MAKEGROUP(fh, "meta") = 0);
PRINT "|at_s="; (HDF5.H5WRITEATTRSTR(fh, "serie", "unit", "eur") = 0);
PRINT "|at_f="; (HDF5.H5WRITEATTRF64(fh, "serie", "scale", 2.5) = 0);
PRINT "|list="; HDF5.H5COUNT(fh); ":";
PRINT HDF5.H5KIND(fh, 0); HDF5.H5KIND(fh, 1); HDF5.H5KIND(fh, 2);
CALL HDF5.H5CLOSE(fh)

' rilettura
fh = HDF5.H5OPEN(P$)
PRINT "|size="; HDF5.H5SIZE(fh, "serie");
DIM rd(5) AS DOUBLE
DIM rdim(7) AS LONG
DIM rn(0) AS LONG
PRINT "|rd_f64="; (HDF5.H5READF64(fh, "serie", rd, rdim, rn) = 0); ":";
PRINT rn(0); ":"; rdim(0); "x"; rdim(1); ":"; rd(0); ","; rd(5);
DIM ri(3) AS LONG
PRINT "|rd_i64="; (HDF5.H5READI64(fh, "conteggi", ri, rdim, rn) = 0); ":"; ri(3);
DIM rz(5) AS DOUBLE
PRINT "|rd_zip="; (HDF5.H5READDBL(fh, "compressa", rz, rdim, rn) = 0); ":"; rz(5);
  ' l'ordine di H5Ovisit dipende dalla build di HDF5: si verifica la PRESENZA del nome
  trovato = 0
  FOR k = 0 TO HDF5.H5COUNT(fh) - 1
    IF HDF5.H5NAME(fh, k) = "serie" THEN trovato = 1
  NEXT k
  PRINT "|nome_serie="; trovato;
PRINT "|attr="; HDF5.H5READATTRSTR(fh, "serie", "unit");
  ' H5READSTR su un dataset NUMERICO: non è un dataset di stringhe -> stringa vuota (con
  ' questo wrapper le stringhe vivono negli attributi; H5READSTR serve ai file di altri tool).
  PRINT "|str_vuota="; (LEN(HDF5.H5READSTR(fh, "serie")) = 0);
PRINT "|ver="; (LEN(HDF5.H5VERSION()) > 0)
CALL HDF5.H5CLOSE(fh)
PRINT ""
END SUB
