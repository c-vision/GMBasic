' MiniTest: membri REALI di STRINGBUILDER (Length/Capacity, anche dopo Clear) e
' forme canoniche al posto del pseudo-membro `.Length`.
' `.Length` non è una proprietà del linguaggio: su stringa/lista/dict/array è un errore
' di compilazione identico sulle 4 gambe (probe LengthPseudoPropertyTests); restano
' legittimi StringBuilder.Length e RegexMatch.Length.
' EXPECT: len= 4|cap= 16|len2= 0|cap2= 16|slen= 4|count= 3|cap1= 32|cap2= 64
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
  ' Crescita della capacità: raddoppio come StringBuilder .NET (max(richiesta, 2*vecchia)).
  DIM sg AS STRINGBUILDER
  sg.Append("0123456789ABCDEFG")            ' 17 caratteri → 32
  PRINT "cap1="; sg.Capacity
  sg.Append("0123456789ABCDEFGHIJ")         ' 37 caratteri → 64
  PRINT "cap2="; sg.Capacity
END SUB
