' MiniTest: membri REALI di STRINGBUILDER (Length/Capacity, anche dopo Clear) e
' forme canoniche al posto del pseudo-membro `.Length`.
' `.Length` non è una proprietà del linguaggio: su stringa/lista/dict/array è un errore
' di compilazione identico sulle 4 gambe (probe LengthPseudoPropertyTests); restano
' legittimi StringBuilder.Length e RegexMatch.Length.
' EXPECT: len= 4|cap= 16|len2= 0|cap2= 16|slen= 4|count= 3
IMPORT STRINGS
SUB Main()
  DIM sb AS STRINGBUILDER
  sb.Append("abcd")
  PRINT "len="; sb.Length
  PRINT "cap="; sb.Capacity
  sb.Clear()
  PRINT "len2="; sb.Length
  PRINT "cap2="; sb.Capacity
  DIM s AS STRING
  s = "abcd"
  PRINT "slen="; LEN(s)
  DIM l = [1, 2, 3]
  PRINT "count="; l.Count
END SUB
