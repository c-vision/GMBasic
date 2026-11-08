' MiniTest NAMESPACE COMPRESSION — tutte le 21 funzioni della superficie.
' Round-trip in memoria per ogni codec (confronto stringa, mai stampa di byte binari),
' più ZIP/UNZIP su file temporanei. Nessun alias di namespace: chiamate qualificate
' per esteso (PCodeVM non risolve `IMPORT ... AS`).
' EXPECT: algorithms_gzip=-1|gzip=ok|zlib=ok|deflate=ok|brotli=ok|lz4=ok|xz=ok|zstd=ok|bzip2=ok|generic=ok|zipfn=ok|zip_lines= 1|zip_content=hello compression
REQUIRE "compression"
IMPORT COMPRESSION
IMPORT IO

SUB Main()
  MKDIR "/tmp/gmb_mt_comp"
  DIM s AS STRING
  s = "OpenCode MiniTest compression 0123456789 abcdefghijklmnopqrstuvwxyz"

  PRINT "algorithms_gzip="; (INSTR(COMPRESSION.ALGORITHMS(), "GZIP") > 0)

  DIM g AS STRING
  g = COMPRESSION.GZIP(s)
  IF COMPRESSION.UNGZIP(g) = s THEN PRINT "gzip=ok" ELSE PRINT "gzip=ko"
  IF COMPRESSION.UNZLIB(COMPRESSION.ZLIB(s)) = s THEN PRINT "zlib=ok" ELSE PRINT "zlib=ko"
  IF COMPRESSION.UNDEFLATE(COMPRESSION.DEFLATE(s)) = s THEN PRINT "deflate=ok" ELSE PRINT "deflate=ko"
  IF COMPRESSION.UNBROTLI(COMPRESSION.BROTLI(s)) = s THEN PRINT "brotli=ok" ELSE PRINT "brotli=ko"
  IF COMPRESSION.UNLZ4(COMPRESSION.LZ4(s)) = s THEN PRINT "lz4=ok" ELSE PRINT "lz4=ko"
  IF COMPRESSION.UNXZ(COMPRESSION.XZ(s)) = s THEN PRINT "xz=ok" ELSE PRINT "xz=ko"
  IF COMPRESSION.UNZSTD(COMPRESSION.ZSTD(s)) = s THEN PRINT "zstd=ok" ELSE PRINT "zstd=ko"
  IF COMPRESSION.UNBZIP2(COMPRESSION.BZIP2(s)) = s THEN PRINT "bzip2=ok" ELSE PRINT "bzip2=ko"

  DIM gc AS STRING
  gc = COMPRESSION.COMPRESS(s, "GZIP")
  IF COMPRESSION.DECOMPRESS(gc, "GZIP") = s THEN PRINT "generic=ok" ELSE PRINT "generic=ko"

  DIM src AS STRING
  src = "/tmp/gmb_mt_comp/src.txt"
  DIM arc AS STRING
  arc = "/tmp/gmb_mt_comp/arc.zip"
  DIM outdir AS STRING
  outdir = "/tmp/gmb_mt_comp/out"
  IF IO.FILEEXISTS(arc) THEN KILL arc
  DIM h AS LONG
  h = IO.FCREATE(src)
  IO.FWRITE(h, "hello compression" + CHR(10))
  IO.FCLOSE(h)

  ' ZIP/UNZIP restano builtin in forma COMANDO. La forma funzione COMPRESSION.ZIP/UNZIP
  ' è stata rimossa con la superficie del namespace (COMPRESSION ora è del pacchetto FFI).
  ZIP src TO arc
  UNZIP arc TO outdir
  IF IO.FILEEXISTS(outdir + "/src.txt") <> 0 THEN PRINT "zipfn=ok" ELSE PRINT "zipfn=ko"
  DIM lines = IO.READLINES(outdir + "/src.txt")
  PRINT "zip_lines="; UBOUND(lines) + 1
  PRINT "zip_content="; lines(0)

  KILL src
  KILL arc
END SUB
