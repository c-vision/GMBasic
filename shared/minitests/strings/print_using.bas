' PRINT USING (riferimento GW-BASIC): campi numerici e di stringa, dollaro e asterischi,
' segni iniziali e finali, overflow dichiarato con '%', letterali, escape '_', ripetizione
' della riga di formato quando i campi finiscono, e convivenza con PRINT "…"; (print_hold).
' Il motore è nel runtime condiviso (gmb_print_using.c), non nell'emitter.
' EXPECT: [  3.14]|[12,345.60]|[  $12.34]|[ -$12.34]|[****$5]|[  +5]|[  5-]|[%1234]|[ciao]|[c]|Nome: Ana (  7)|solo testo|[#]|[ 3.14]|[  1]|[  2]|[  5]
SUB Main()
  DIM f AS STRING = "[##.##]"
  PRINT USING "[###.##]"; 3.14159
  PRINT USING "[##,###.##]"; 12345.6
  PRINT USING "[$$###.##]"; 12.34
  PRINT USING "[$$###.##]"; -12.34
  PRINT USING "[**$###]"; 5
  PRINT USING "[+###]"; 5
  PRINT USING "[###-]"; -5
  PRINT USING "[##]"; 1234
  PRINT USING "[&]"; "ciao"
  PRINT USING "[!]"; "ciao"
  PRINT USING "Nome: & (###)"; "Ana"; 7
  PRINT USING "solo testo"
  PRINT USING "[_#]"
  PRINT USING f; 3.14159
  PRINT USING "[###]"; 1; 2
  PRINT "[";
  PRINT USING "###"; 5;
  PRINT "]"
END SUB
