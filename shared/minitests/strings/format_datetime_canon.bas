' FORMAT data/ora: canone unico sui 4 backend (Strings/StringsFormat.bas): token case-sensitive (M mese, m/n minuti,
' h 12h, H 24h), t/tt e AM/PM, '' dentro un letterale = apostrofo, nomi giorno/mese in lingua invariante (inglese),
' sezioni booleane con spazi rispettati.
' EXPECT: [Thursday, 25 December 2025]|[Thu 25 Dec 25]|[22:05:30]|[10:05:30 PM]|[10:05 PM P]|[30 30]|[2025 dell'anno]|[ Si ]|[No]
SUB Main()
  DIM d AS DOUBLE = DATESERIAL(2025, 12, 25) + TIMESERIAL(22, 5, 30)
  PRINT "["; FORMAT(d, "dddd, d MMMM yyyy"); "]"
  PRINT "["; FORMAT(d, "ddd d MMM yy"); "]"
  PRINT "["; FORMAT(d, "HH:mm:ss"); "]"
  PRINT "["; FORMAT(d, "hh:mm:ss tt"); "]"
  PRINT "["; FORMAT(d, "h:nn AM/PM t"); "]"
  PRINT "["; FORMAT(d, "ss s"); "]"
  PRINT "["; FORMAT(d, "yyyy 'dell''anno'"); "]"
  PRINT "["; FORMAT(-1, " Si ; No "); "]"
  PRINT "["; FORMAT(0, " Si ;No"); "]"
END SUB
