' MiniTest VECTORDB — PURGEDEPRECATED (compilazione + esecuzione).
' L'output dipende dal provider embedding attivo (fixture/reale).
' Senza EXPECT: testa che la funzione compili e non crashi su tutti i backend.
IMPORT VECTORDB

SUB Main()
    VECTORDB.ENSURECOLLECTION(":memory:", "pd_smoke", 4)
    DIM p = VECTORDB.PURGEDEPRECATED(":memory:", "pd_smoke")
END SUB
