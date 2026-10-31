' MiniTest: JSON.TOOBJECT(LIST OF INTEGER, json) con TARGET BOXED (var AS JSON).
' Il parser C appiattisce LIST OF <T> al nome dell'elemento, quindi il backend C99Native
' deve scegliere la forma (gmb_val_as_list vs gmb_json_parse) dal tipo del TARGET.
' EXPECT: json=[10,20,30]
IMPORT JSON

SUB Main()
  DIM lst AS LIST OF INTEGER = [10, 20, 30]
  DIM lj AS JSON = JSON.TOJSON(lst)
  DIM x AS JSON
  x = JSON.TOOBJECT(LIST OF INTEGER, lj)
  PRINT "json="; JSON.STRINGIFY(x)
END SUB
