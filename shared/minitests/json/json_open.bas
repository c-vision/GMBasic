' MiniTest: JSON OPEN + PRINT #h (I/O a handle) — parità 4 backend.
' Sul runtime IL il PRINT #h scriveva il ToString() del CLR (System.Collections.Generic.
' Dictionary`2[...]) invece del JSON → JSON.LOAD falliva; ora, quando l'handle è JSON
' (content type 1, come task.FileContentTypes del PCodeVM), l'argomento viene serializzato
' come JSON (e per gli handle non-JSON resta il Convert.ToString di prima).
' EXPECT: w1={"n":42}|r1= 42
IMPORT JSON
IMPORT IO

SUB Main()
  DIM h AS LONG
  DIM q AS STRING
  q = CHR(34)
  DIM doc AS JSON = JSON.PARSE("{" + q + "n" + q + ": 42}")
  JSON OPEN "jopen.json" FOR "WRITE" AS #h
  PRINT #h, doc
  CLOSE #h
  DIM back AS JSON = JSON.LOAD("jopen.json")
  PRINT "w1="; JSON.STRINGIFY(back)
  PRINT "r1="; JSON.EXTRACT(back, "n")
  KILL "jopen.json"
END SUB
