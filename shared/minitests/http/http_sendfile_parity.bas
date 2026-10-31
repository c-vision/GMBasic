' MiniTest HTTP — parità 4 backend per res.SendFile(path [, contentType]).
' Copre le tre divergenze corrette (2026-10-16):
'   - corpo TESTO servito (base);
'   - corpo BINARIO con byte NUL: non deve essere troncato (usava strlen);
'   - STATUS 500 su file mancante (il bridge C99 leggeva la chiave sbagliata → 200).
' Il file binario è creato dal test come array di BYTE (PUT # su file BINARY): così il NUL è
' reale e il test è portabile senza dipendere da una fixture del repo. Path ASSOLUTI (/tmp)
' perché la CWD differisce fra le gambe.
' EXPECT: txt-ok|bin-ok|miss-500
IMPORT HTTP
IMPORT IO

SUB OnTxt(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.SendFile("/tmp/gmb_mt_sf_txt.dat")
END SUB

SUB OnBin(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.SendFile("/tmp/gmb_mt_sf_bin.dat")
END SUB

SUB OnMiss(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.SendFile("/tmp/gmb_mt_sf_missing.dat")
END SUB

SUB Main()
  ' 1) file di testo
  OPEN "/tmp/gmb_mt_sf_txt.dat" FOR WRITE AS #1
  PRINT #1, "SENDFILE-OK"
  CLOSE #1

  ' 2) file binario con NUL (header ZIP-like: 'P','K',3,4,0,0,0,0,7,8 → 10 byte)
  DIM B(9) AS BYTE
  B(0) = 80 : B(1) = 75 : B(2) = 3 : B(3) = 4 : B(4) = 0
  B(5) = 0 : B(6) = 0 : B(7) = 0 : B(8) = 7 : B(9) = 8
  OPEN "/tmp/gmb_mt_sf_bin.dat" FOR BINARY AS #2
  PUT #2, , B
  CLOSE #2

  DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
  cfg.Port = 8798
  HTTP CONFIG cfg
  HTTP ROUTE "GET", "/t", OnTxt()
  HTTP ROUTE "GET", "/b", OnBin()
  HTTP ROUTE "GET", "/m", OnMiss()
  HTTP SERVER 8798
  SLEEP 800

  DIM rt AS STRING
  rt = HTTP.GET("http://localhost:8798/t")
  IF INSTR(rt, "SENDFILE-OK") > 0 THEN PRINT "txt-ok" ELSE PRINT "txt-fail"

  DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
  opts.Headers.Connection = "close"
  DIM fb AS FETCHRESULT = FETCH("http://localhost:8798/b", opts)
  IF fb.ContentLength = 10 THEN PRINT "bin-ok" ELSE PRINT "bin-fail-cl="; fb.ContentLength

  DIM fm AS FETCHRESULT = FETCH("http://localhost:8798/m", opts)
  IF fm.StatusCode = 500 THEN PRINT "miss-500" ELSE PRINT "miss-fail-st="; fm.StatusCode

  HTTP SERVER STOP
  KILL "/tmp/gmb_mt_sf_txt.dat"
  KILL "/tmp/gmb_mt_sf_bin.dat"
END SUB
