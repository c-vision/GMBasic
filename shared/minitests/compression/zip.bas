' MiniTest: COMPRESSION.ZIP/UNZIP (comandi) — archivio store-format, round-trip.
' Path assoluti (la CWD differisce tra i backend).
' EXPECT: lines:  1|content: hello zip
IMPORT IO
REQUIRE "compression"
IMPORT COMPRESSION AS C

SUB Main()
  src$ = "/tmp/gmb_mt_zip_src.txt"
  arc$ = "/tmp/gmb_mt_zip_arc.zip"
  outdir$ = "/tmp/gmb_mt_zip_out"

  DIM h AS LONG
  h = IO.FCREATE(src$)
  IO.FWRITE(h, "hello zip" + CHR(10))
  IO.FCLOSE(h)

  ZIP src$ TO arc$
  UNZIP arc$ TO outdir$

  DIM lines = IO.READLINES(outdir$ + "/gmb_mt_zip_src.txt")
  PRINT "lines: "; UBOUND(lines) + 1
  PRINT "content: "; lines(0)
END SUB
