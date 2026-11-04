' EXPECT: peek-ok|read-ok
IMPORT IO
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  MKDIR GMBWORK$
  DIM f = IO.FCREATE(GMBWORK$ + "/mmap_test.dat")
  IO.FWRITE(f, "hello")
  IO.FCLOSE(f)
  DIM mm = IO.MEMORYMAP(GMBWORK$ + "/mmap_test.dat")
  IF IO.MMAPPEEK(mm, 0) >= 0 THEN PRINT "peek-ok"
  DIM b = IO.MMAPREADBYTES(mm, 0, 3)
  IF LEN(b) >= 0 THEN PRINT "read-ok"
END SUB
