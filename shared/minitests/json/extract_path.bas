' EXPECT: name=Ada|tag=home|tag2=dev
IMPORT JSON
SUB Main()
  ' JSON.EXTRACT con path ANNIDATO (.k) e INDICE ARRAY ([i]) — F2: walk dei
  ' segmenti ora unico su 4 backend (builtin VM 765 delega a 3407; C99Native
  ' gmb_json_extract runtime; IL/PCode PCodeJsonExtractPath*).
  ' NOTA: [i].key misto (es. items[1].sku) pianta la pipeline emitter C#
  ' (hang in Orchestrator/emitter, bug preesistente tracciato) — usare [i] solo
  ' come segmento FINALE o chiavi annidate.
  DIM q$ = CHR(34)
  DIM src$ = "{" + q$ + "user" + q$ + ":{" + q$ + "name" + q$ + ":" + q$ + "Ada" + q$ + "}," + q$ + "tags" + q$ + ":[" + q$ + "home" + q$ + "," + q$ + "dev" + q$ + "]}"
  DIM o = JSON.PARSE(src$)
  PRINT "name="; JSON.EXTRACT(o, "user.name")
  PRINT "tag="; JSON.EXTRACT(o, "tags[0]")
  PRINT "tag2="; JSON.EXTRACT(o, "tags[1]")
END SUB
