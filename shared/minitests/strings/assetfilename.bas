' EXPECT: back_end_x_y_z_w|dotted..|plain|abc_def
' Contratto del sanificatore dei nomi file (AssetFileName in
' samples/Community/PackageRegistry/PackageServer.bas): neutralizza / spazio,
' backslash, ':' e '@' perche' la chiave di un asset ("backend/platform") diventi
' un nome file valido anche su Windows.
'
' REGRESSIONE STORICA (2026-10-28): il confronto col backslash era scritto come
' letterale di DUE caratteri, quindi non poteva mai combaciare con un char singolo
' (ramo morto: il backslash restava nella chiave). Il carattere va scritto come
' CHR$(92); il primo caso di questo test e' esattamente quella regressione.
IMPORT IO

FUNCTION AssetFileName(k AS STRING) AS STRING
  DIM si AS LONG, cc AS STRING
  FOR si = 1 TO LEN(k)
    cc = MID(k, si, 1)
    IF cc = "/" OR cc = " " OR cc = CHR$(92) OR cc = ":" OR cc = "@" THEN k = LEFT(k, si - 1) + "_" + MID(k, si + 1)
  NEXT si
  RETURN k
END FUNCTION

PRINT AssetFileName("back" + CHR$(92) + "end/x y:z@w")
PRINT AssetFileName("dotted..")
PRINT AssetFileName("plain")
PRINT AssetFileName("abc def")
