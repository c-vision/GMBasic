' MiniTest NAMESPACE MEMORYMAP — MEMORYMAP, MMAPPEEK, MMAPREADBYTES.
' MMAPPOKE (comando) prepara i byte della mappa.
' EXPECT: peek= 65|read= 2 | 65 | 66
IMPORT MEMORYMAP
IMPORT IO
SUB Main()
  MKDIR "work"
  DIM p AS STRING
  p = "work/mmap_all.dat"
  KILL p
  DIM h AS LONG
  h = MEMORYMAP.MEMORYMAP(p)
  MMAPPOKE h, 0, 65
  MMAPPOKE h, 1, 66
  PRINT "peek="; MEMORYMAP.MMAPPEEK(h, 0)
  DIM b = MEMORYMAP.MMAPREADBYTES(h, 0, 2)
  PRINT "read="; UBOUND(b) + 1; "|"; b(0); "|"; b(1)
  CLOSE h
  KILL p
END SUB
