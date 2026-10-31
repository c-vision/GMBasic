' MiniTest HTTP — `FETCH(...)` + `FETCHRESULT.SaveToFile(path)`: download BINARIO su 4 gambe.
'
' Bug trovato scrivendo questo test (2026-10-30): su C99Native `r.SaveToFile(path)` finiva nel
' ramo JSON `base.Membro[arg]` dell'emitter (`gmb_val_member_or_index(r,"SaveToFile",path)`) e
' non creava NULLA: il file restava da 0 byte, mentre IL/PCode/VM-C scrivevano i byte giusti.
' Il caso non era mai stato coperto: http_sendfile_parity.bas prova il SERVER (res.SendFile),
' mai il CLIENT che salva su disco.
'
' Il payload è deterministico e creato dal test (array di BYTE → PUT # su file BINARY):
'   - 256 byte: tutti i valori 0..255 (NUL, CR, LF, 0xFF inclusi);
'   - 4096 byte: (k * 7 + 3) MOD 256 ripetuto su 16 blocchi (più blocchi di lettura, NUL sparsi).
' Dopo il download si rilegge il file con GET # e si confronta BYTE per BYTE (non basta la
' dimensione: un file pieno di zeri della lunghezza giusta passerebbe).
' Nota: gli indici dei due cicli restano 0..255 e il valore calcolato passa da una variabile
' LONG prima dello store (un'espressione scritta direttamente nella cella di un array di BYTE
' viene ristretta a BYTE dal linker).
' EXPECT: cl-small-256|save-small-ok|bytes-small-ok|cl-big-4096|save-big-ok|bytes-big-ok
IMPORT HTTP
IMPORT IO

SUB OnSmall(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.SendFile("/tmp/gmb_mt_fetchbin_small.dat")
END SUB

SUB OnBig(req AS HTTPREQUEST, res AS HTTPRESPONSE)
  res.SendFile("/tmp/gmb_mt_fetchbin_big.dat")
END SUB

SUB Main()
  DIM i AS LONG, k AS LONG, b AS LONG, v AS LONG

  ' ── payload 1: 256 byte, tutti i valori ────────────────────────────────────────────────
  DIM p(255) AS BYTE
  FOR i = 0 TO 255
    v = i
    p(i) = v
  NEXT i
  IF FILEEXISTS("/tmp/gmb_mt_fetchbin_small.dat") THEN KILL "/tmp/gmb_mt_fetchbin_small.dat"
  OPEN "/tmp/gmb_mt_fetchbin_small.dat" FOR BINARY AS #1
  PUT #1, , p
  CLOSE #1

  ' ── payload 2: 4096 byte, pattern con NUL (16 blocchi da 256) ─────────────────────────
  ' v = ... in un LONG prima dello store: un'espressione aritmetica scritta direttamente nella
  ' cella di un array di BYTE viene ristretta a BYTE dal linker (k*7+3 può superare 255) e il
  ' compilatore la rifiuta (err. overflow) — qui il valore è già 0..255.
  DIM q(4095) AS BYTE
  FOR b = 0 TO 15
    FOR k = 0 TO 255
      v = (k * 7 + 3) MOD 256
      q(b * 256 + k) = v
    NEXT k
  NEXT b
  IF FILEEXISTS("/tmp/gmb_mt_fetchbin_big.dat") THEN KILL "/tmp/gmb_mt_fetchbin_big.dat"
  OPEN "/tmp/gmb_mt_fetchbin_big.dat" FOR BINARY AS #2
  PUT #2, , q
  CLOSE #2

  DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
  cfg.Port = 8801
  HTTP CONFIG cfg
  HTTP ROUTE "GET", "/small", OnSmall()
  HTTP ROUTE "GET", "/big", OnBig()
  HTTP SERVER 8801
  SLEEP 800

  DIM opts AS HTTPOPTIONS = NEW HTTPOPTIONS
  opts.Headers.Connection = "close"

  ' ── small: 256 byte ───────────────────────────────────────────────────────────────────
  IF FILEEXISTS("/tmp/gmb_mt_fetchbin_small.out") THEN KILL "/tmp/gmb_mt_fetchbin_small.out"
  DIM rs AS FETCHRESULT = FETCH("http://localhost:8801/small", opts)
  IF rs.ContentLength = 256 THEN PRINT "cl-small-256" ELSE PRINT "cl-small-fail:" + STR$(rs.ContentLength)
  rs.SaveToFile("/tmp/gmb_mt_fetchbin_small.out")
  DIM g1(255) AS BYTE
  DIM n AS LONG, dif AS LONG
  IF NOT FILEEXISTS("/tmp/gmb_mt_fetchbin_small.out") THEN
    PRINT "save-small-missing"
  ELSE
    OPEN "/tmp/gmb_mt_fetchbin_small.out" FOR BINARY AS #5
    n = IO.LOF(5)
    GET #5, , g1
    CLOSE #5
    IF n = 256 THEN PRINT "save-small-ok" ELSE PRINT "save-small-size:" + STR$(n)
    dif = 0
    FOR i = 0 TO 255
      IF g1(i) <> p(i) THEN dif = dif + 1
    NEXT i
    IF dif = 0 THEN PRINT "bytes-small-ok" ELSE PRINT "bytes-small-diff:" + STR$(dif)
  END IF

  ' ── big: 4096 byte ────────────────────────────────────────────────────────────────────
  IF FILEEXISTS("/tmp/gmb_mt_fetchbin_big.out") THEN KILL "/tmp/gmb_mt_fetchbin_big.out"
  DIM rb AS FETCHRESULT = FETCH("http://localhost:8801/big", opts)
  IF rb.ContentLength = 4096 THEN PRINT "cl-big-4096" ELSE PRINT "cl-big-fail:" + STR$(rb.ContentLength)
  rb.SaveToFile("/tmp/gmb_mt_fetchbin_big.out")
  DIM g2(4095) AS BYTE
  IF NOT FILEEXISTS("/tmp/gmb_mt_fetchbin_big.out") THEN
    PRINT "save-big-missing"
  ELSE
    OPEN "/tmp/gmb_mt_fetchbin_big.out" FOR BINARY AS #6
    n = IO.LOF(6)
    GET #6, , g2
    CLOSE #6
    IF n = 4096 THEN PRINT "save-big-ok" ELSE PRINT "save-big-size:" + STR$(n)
    dif = 0
    FOR i = 0 TO 4095
      IF g2(i) <> q(i) THEN dif = dif + 1
    NEXT i
    IF dif = 0 THEN PRINT "bytes-big-ok" ELSE PRINT "bytes-big-diff:" + STR$(dif)
  END IF

  HTTP SERVER STOP
  KILL "/tmp/gmb_mt_fetchbin_small.dat"
  KILL "/tmp/gmb_mt_fetchbin_big.dat"
  KILL "/tmp/gmb_mt_fetchbin_small.out"
  KILL "/tmp/gmb_mt_fetchbin_big.out"
END SUB
