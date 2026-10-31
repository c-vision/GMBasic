' MiniTest: Stream D / D3 — TRAIN DISTILL RESTORE (backup -> ckpt) — parita' 4 backend.
' Crea un backup fixture (<bk>.src con il ckpt di origine + <bk>_{A,B}.raw) e verifica che
' RESTORE ricopi i file al checkpoint (<ck>_best_{A,B}.raw). Non richiede il modello.
' EXPECT: restore: 1|both: 1
IMPORT AI
IMPORT IO

SUB Main()
  DIM bk AS STRING = "/tmp/gmb_d3_restore_bk"
  DIM ck AS STRING = "/tmp/gmb_d3_restore_ck"

  ' fixture: <bk>.src (path ckpt originale) + <bk>_{A,B}.raw
  DIM hs AS LONG = IO.FCREATE(bk + ".src")
  IO.FWRITE(hs, ck + CHR(10))
  IO.FCLOSE(hs)
  DIM ha AS LONG = IO.FCREATE(bk + "_A.raw")
  IO.FWRITE(ha, "AAAA")
  IO.FCLOSE(ha)
  DIM hb AS LONG = IO.FCREATE(bk + "_B.raw")
  IO.FWRITE(hb, "BBBB")
  IO.FCLOSE(hb)

  TRAIN DISTILL RESTORE bk
  IF IO.FILEEXISTS(ck + "_best_A.raw") THEN PRINT "restore: 1" ELSE PRINT "restore: 0"
  IF IO.FILEEXISTS(ck + "_best_A.raw") AND IO.FILEEXISTS(ck + "_best_B.raw") THEN PRINT "both: 1" ELSE PRINT "both: 0"

  ON ERROR RESUME NEXT
  KILL bk + ".src"
  KILL bk + "_A.raw"
  KILL bk + "_B.raw"
  KILL ck + "_best_A.raw"
  KILL ck + "_best_B.raw"
  ON ERROR GOTO 0
END SUB
