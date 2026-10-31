' MiniTest: ASSEGNAZIONE a variabile SCALARE da ESPRESSIONE (2026-10-10).
' Regressione del vecchio emitter C# (assegnazione ANY da espressione -> gmb_val_nil):
' il linker non inferisce il tipo dell'ESPRESSIONE (`okc = (x = 1)`, `okc = x + 2`,
' `okc = (doc.user = "Ada")`) e in quei casi marca ResolvedType (e il tipo del simbolo)
' ANY. L'emitter C# cadeva allora nel ramo gmb_val_t di EmitValueWithType ed emetteva
' `okc = gmb_val_nil();` — l'espressione era SCARTATA e la variabile (dichiarata
' gmb_int_t) restava 0, senza alcun errore di compilazione: parità con
' IL/PCode/VM-C/C99Native rotta in silenzio.
' Qui si verifica il VALORE effettivamente assegnato (non solo l'assenza di crash) e la
' forma "variabile GIÀ assegnata e poi sovrascritta da un'espressione".
' Canone BASIC: il confronto vale -1 (vero) / 0 (falso).
' EXPECT: int_cmp=-1 |int_cmp_false= 0 |str_cmp=-1 |str_cmp_false= 0 |int_arith= 7 |overwrite=-1 |dbl_div= 2.5 |str_concat=q!
SUB Main()
  DIM x AS INTEGER
  DIM y AS INTEGER
  DIM s AS STRING
  x = 5
  y = 2
  s = "q"

  ' 1. confronto numerico → INTEGER
  DIM a1 AS INTEGER
  a1 = (x > y)

  ' 2. confronto falso → 0
  DIM a2 AS INTEGER
  a2 = (x < y)

  ' 3. confronto di STRINGHE → INTEGER
  DIM a3 AS INTEGER
  a3 = (s = "q")

  ' 4. confronto di stringhe falso → 0
  DIM a4 AS INTEGER
  a4 = (s = "z")

  ' 5. aritmetica → INTEGER
  DIM a5 AS INTEGER
  a5 = x + y

  ' 6. variabile già assegnata e POI sovrascritta da un confronto composto
  DIM a6 AS INTEGER
  a6 = 0
  a6 = (x = 5) AND (y = 2)

  ' 7. espressione → DOUBLE (divisione BASIC: sempre frazionaria)
  DIM a7 AS DOUBLE
  a7 = x / y

  ' 8. espressione → STRING (concatenazione)
  DIM a8 AS STRING
  a8 = s + "!"

  PRINT "int_cmp="; a1;
  PRINT "|int_cmp_false="; a2;
  PRINT "|str_cmp="; a3;
  PRINT "|str_cmp_false="; a4;
  PRINT "|int_arith="; a5;
  PRINT "|overwrite="; a6;
  PRINT "|dbl_div="; a7;
  PRINT "|str_concat="; a8
END SUB
