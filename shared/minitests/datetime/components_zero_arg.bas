' MiniTest: componenti DATETIME con ZERO argomenti = data/ora CORRENTI
' (`YEAR()` ≡ YEAR(NOW())), sia con parentesi sia in forma nuda.
' Prima: il linker C# rispondeva "Simbolo non definito" (nessuna overload a 0 arg),
' C99 dava 0 / errore di arità. Asserzioni STABILI (range), non valori assoluti.
' EXPECT: y=-1|mo=-1|d=-1|h=-1|mi=-1|s=-1|w=-1|nuda=-1
SUB Main()
  PRINT "y="; (YEAR() >= 2024)
  PRINT "mo="; (MONTH() >= 1) AND (MONTH() <= 12)
  PRINT "d="; (DAY() >= 1) AND (DAY() <= 31)
  PRINT "h="; (HOUR() >= 0) AND (HOUR() <= 23)
  PRINT "mi="; (MINUTE() >= 0) AND (MINUTE() <= 59)
  PRINT "s="; (SECOND() >= 0) AND (SECOND() <= 60)
  PRINT "w="; (WEEKDAY() >= 1) AND (WEEKDAY() <= 7)
  PRINT "nuda="; (YEAR >= 2024) AND (MONTH >= 1)
END SUB
