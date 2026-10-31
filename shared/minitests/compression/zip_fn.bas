' MiniTest: COMPRESSION.ZIP/UNZIP in FORMA FUNZIONE (valore BIT di ritorno).
' Forma comando (ZIP src TO arc) coperta da zip.bas; qui le funzioni omonime
' ok = COMPRESSION.ZIP(sorgenti$, dest$) / COMPRESSION.UNZIP(archivio$, cartella$).
' Path assoluti (la CWD differisce tra i backend).
' EXPECT: zipfn-ok|lines:  1|content: hello zipfn
IMPORT IO
IMPORT COMPRESSION AS C

SUB Main()
  src$ = "/tmp/gmb_mt_zipfn_src.txt"
  arc$ = "/tmp/gmb_mt_zipfn_arc.zip"
  outdir$ = "/tmp/gmb_mt_zipfn_out"
  ' Cleanup deterministico: senza, l'archivio lasciato da una gamba fa fallire la
  ' successiva (ZIP su archivio esistente → "zipfn-ko"). Percorsi assoluti perché la
  ' CWD differisce tra i backend.
  IF IO.FILEEXISTS(arc$) THEN KILL arc$
  IF IO.FILEEXISTS(src$) THEN KILL src$

  DIM h AS LONG
  h = IO.FCREATE(src$)
  IO.FWRITE(h, "hello zipfn" + CHR(10))
  IO.FCLOSE(h)

  DIM okz = C.ZIP(src$, arc$)
  DIM oku = C.UNZIP(arc$, outdir$)
  IF okz <> 0 AND oku <> 0 THEN PRINT "zipfn-ok" ELSE PRINT "zipfn-ko"

  DIM lines = IO.READLINES(outdir$ + "/gmb_mt_zipfn_src.txt")
  PRINT "lines: "; UBOUND(lines) + 1
  PRINT "content: "; lines(0)
END SUB
