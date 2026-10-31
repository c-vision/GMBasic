' EXPECT: 23|name=Ada
IMPORT JSON
SUB Main()
  ' Parse annidato + JSON.EXTRACT con path a segmenti (.k, [i]) su 4 backend.
  ' F2: unica implementazione del parse nel runtime C (gmb_json_parse); VM/C
  ' delega e converte il box runtime in wrapper VM (kind GMB_OBJ_*).
  ' JSON.EXTRACT: builtin VM 765 delega al walk di 3407 (era key get piatto).
  DIM q AS STRING = CHR(34)
  DIM src = "{" + q + "user" + q + ":{" + q + "name" + q + ":" + q + "Ada" + q + "}}"
  PRINT LEN(src)
  DIM o = JSON.PARSE(src)
  PRINT "name="; JSON.EXTRACT(o, "user.name")
END SUB
