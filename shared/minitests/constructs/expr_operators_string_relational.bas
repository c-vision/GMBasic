' MiniTest: confronto relazionale tra stringhe (bug 2026-10-01).
' Due stringhe si confrontano per ordinale lessicografico (canone IL/PCodeVM
' string.CompareOrdinal, runtime condiviso gmb_string_compare): vero = -1.
' Casi di confine: case-sensitive ("ABCdef" > "abcdef" e' falso), numerico
' ("10" > "9" e' falso: ordinale, non numerico), prefissi ("abcdef" < "abs").
' EXPECT: strrel-ok
SUB Main()
  DIM ok AS INTEGER = -1
  IF ("a" < "b") <> -1 THEN ok = 0
  IF ("b" > "a") <> -1 THEN ok = 0
  IF ("a" <= "a") <> -1 THEN ok = 0
  IF ("a" >= "a") <> -1 THEN ok = 0
  IF ("b" < "a") <> 0 THEN ok = 0
  IF ("a" > "b") <> 0 THEN ok = 0
  IF ("b" <= "a") <> 0 THEN ok = 0
  IF ("a" >= "b") <> 0 THEN ok = 0
  IF ("abcdef" < "abs") <> -1 THEN ok = 0
  IF ("abcdef" > "abs") <> 0 THEN ok = 0
  IF ("ABCdef" > "abcdef") <> 0 THEN ok = 0
  IF ("10" > "9") <> 0 THEN ok = 0
  IF ("abc" <= "abc") <> -1 THEN ok = 0
  IF ("abc" >= "abc") <> -1 THEN ok = 0
  IF ("abc" = "abc") <> -1 THEN ok = 0
  IF ("abc" <> "abd") <> -1 THEN ok = 0
  IF (("abc") <= ("abc")) <> -1 THEN ok = 0
  IF (("abc") >= ("abc")) <> -1 THEN ok = 0
  IF (("al" + "pha") = "alpha") <> -1 THEN ok = 0
  DIM gv AS STRING
  gv = "beta"
  IF (("al" + "pha") < gv) <> -1 THEN ok = 0
  IF ((UPPER("abc")) > "ABB") <> -1 THEN ok = 0
  IF ((UPPER("abc")) = "ABC") <> -1 THEN ok = 0
  IF ok THEN PRINT "strrel-ok"
END SUB
