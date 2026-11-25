' MiniTest variabili il cui nome è un TOKEN di builtin (PI, COUNT, LENGTH, KEYS).
'
' REGRESSIONE (2026-11-24): il dispatch per TokenKind vinceva sulla variabile DICHIARATA.
' Il lexer assegna al nome il token corrispondente (LIST/KEYS/COUNT/VALUES/PI sono token) e:
'  - C99Native: `count(2)` (elemento di un array `count()` dichiarato) finiva sul builtin COUNT
'    o su gmb_list_count; `LIST$(0)` su gmb_provider_list_names; `LEN(keys(0))` diventava
'    gmb_list_count(keys[...]) (C non compilabile);
'  - PCodeVM/VM-C: `_constants` è OrdinalIgnoreCase e contiene "PI" → `DIM pi = 1.5` restava π.
' Regola (la stessa delle keyword, vedi keyword_named_var.bas): una VARIABILE dichiarata
' vince sul builtin omonimo.
' EXPECT: pi= 1.5|count= 2|length= 3|k0|k2|ub= 2|j=k0-k1-k2|len0= 2
SUB Main()
  DIM pi = 1.5
  DIM count = 2
  DIM length = 3
  DIM keys(2) AS STRING

  keys(0) = "k0"
  keys(1) = "k1"
  keys(2) = "k2"

  PRINT "pi="; pi
  PRINT "count="; count
  PRINT "length="; length
  PRINT keys(0)
  PRINT keys(2)
  PRINT "ub="; UBOUND(keys)
  PRINT "j="; JOIN(keys, "-")
  PRINT "len0="; LEN(keys(0))
END SUB
