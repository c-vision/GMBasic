' ============================================================================
' COMPRESSION — round-trip di ogni algoritmo + archivio ZIP.
' Esempio per il namespace COMPRESSION (GZIP/BROTLI/ZLIB/BZIP2/DEFLATE + ZIP).
' Path assoluti: la CWD differisce tra i backend.
' ============================================================================
REQUIRE "compression"
IMPORT COMPRESSION AS C
IMPORT IO

SUB Main()
  s$ = "The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog."

  PRINT "algoritmi: "; C.ALGORITHMS()

  PRINT "gzip:    "; C.UNGZIP(C.GZIP(s$))
  PRINT "brotli:  "; C.UNBROTLI(C.BROTLI(s$))
  PRINT "zlib:    "; C.UNZLIB(C.ZLIB(s$))
  PRINT "bzip2:   "; C.UNBZIP2(C.BZIP2(s$))
  PRINT "deflate: "; C.UNDEFLATE(C.DEFLATE(s$))

  ' Forma generica COMPRESS(data, algoritmo) / DECOMPRESS(data, algoritmo).
  PRINT "generico:"; C.DECOMPRESS(C.COMPRESS(s$, "GZIP"), "GZIP")

  ' Archivio ZIP (store-format) dei file elencati.
  DIM src AS STRING
  src = "/tmp/gmb_compression_sample.txt"
  DIM h AS LONG
  h = IO.FCREATE(src)
  IO.FWRITE(h, "contenuto archivio" + CHR(10))
  IO.FCLOSE(h)
  ZIP src TO "/tmp/gmb_compression_sample.zip"
  UNZIP "/tmp/gmb_compression_sample.zip" TO "/tmp/gmb_compression_sample_out"
  DIM lines = IO.READLINES("/tmp/gmb_compression_sample_out/gmb_compression_sample.txt")
  PRINT "zip:     "; lines(0)
  KILL src
END SUB
