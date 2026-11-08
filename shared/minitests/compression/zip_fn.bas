' MiniTest: ZIP/UNZIP (comandi builtin) con PIU' SORGENTI (lista separata da ';').
' La forma FUNZIONE COMPRESSION.ZIP/UNZIP e' stata rimossa insieme alla superficie del
' namespace COMPRESSION (ora del pacchetto FFI); i comandi ZIP/UNZIP restano builtin del
' runtime. zip.bas copre la sorgente singola; qui si verifica l'archivio multi-file.
' Path assoluti (la CWD differisce tra i backend).
' EXPECT: zipfn-ok|a: hello A|b: hello B
IMPORT IO

SUB Main()
  srcA$ = "/tmp/gmb_mt_zipfn_a.txt"
  srcB$ = "/tmp/gmb_mt_zipfn_b.txt"
  arc$ = "/tmp/gmb_mt_zipfn_arc.zip"
  outdir$ = "/tmp/gmb_mt_zipfn_out"
  ' Cleanup deterministico: un archivio lasciato da una gamba precedente farebbe
  ' fallire la successiva (ZIP su archivio esistente).
  IF IO.FILEEXISTS(arc$) THEN KILL arc$

  DIM h AS LONG
  h = IO.FCREATE(srcA$)
  IO.FWRITE(h, "hello A" + CHR(10))
  IO.FCLOSE(h)
  h = IO.FCREATE(srcB$)
  IO.FWRITE(h, "hello B" + CHR(10))
  IO.FCLOSE(h)

  ZIP srcA$ + ";" + srcB$ TO arc$
  UNZIP arc$ TO outdir$

  IF IO.FILEEXISTS(outdir$ + "/gmb_mt_zipfn_a.txt") <> 0 AND IO.FILEEXISTS(outdir$ + "/gmb_mt_zipfn_b.txt") <> 0 THEN
    PRINT "zipfn-ok"
  ELSE
    PRINT "zipfn-ko"
  END IF
  DIM la = IO.READLINES(outdir$ + "/gmb_mt_zipfn_a.txt")
  DIM lb = IO.READLINES(outdir$ + "/gmb_mt_zipfn_b.txt")
  PRINT "a: "; la(0)
  PRINT "b: "; lb(0)
END SUB
