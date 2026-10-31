' CURRENCY: tipo double a virgola fissa — i valori frazionari NON vengono troncati
' (parità IL/PCode/VM-C/C99Native; emitter C# lo mappava a gmb_int_t).
' EXPECT: c= 3.5|somma= 4.75
SUB Main()
  DIM c AS CURRENCY
  c = 3.5
  PRINT "c="; c
  DIM d AS CURRENCY
  d = 1.25
  PRINT "somma="; c + d
END SUB
