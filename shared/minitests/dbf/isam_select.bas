' MiniTest ISAM SELECT — commuta area ISAM per alias
' Verifica che SELECT commuti l'area: dopo INSERT su A1 e A2, SELECT A1 → LASTREC(A1)=1,
' SELECT A2 → LASTREC(A2)=1.
' NOTA: usa ISAM.LASTREC(alias), non ISAM.GET(alias,rec) — la GET con area esplicita in
' multi-area dà vuoto su PCode/VM/C (gap documentato, vedi bug ISAM multi-area).
' EXPECT: la:  1|lb:  1
IMPORT ISAM

TYPE R
    ID AS STRING * 10
    V AS STRING * 20
END TYPE

DIM rec AS R

SUB Main()
    ON ERROR RESUME NEXT
    KILL "/tmp/gmb_mt_isamsel_a.isdb"
    KILL "/tmp/gmb_mt_isamsel_b.isdb"
    ON ERROR GOTO 0

    ISAM OPEN "/tmp/gmb_mt_isamsel_a.isdb" OF R ALIAS A1 INDEXED BY ID
    ISAM OPEN "/tmp/gmb_mt_isamsel_b.isdb" OF R ALIAS A2 INDEXED BY ID

    ISAM SELECT A1
    rec.ID = "A1-1": rec.V = "primo": ISAM APPEND A1, rec

    ISAM SELECT A2
    rec.ID = "A2-1": rec.V = "solo": ISAM APPEND A2, rec

    ISAM SELECT A1
    PRINT "la: " + STR$(ISAM.LASTREC(A1))

    ISAM SELECT A2
    PRINT "lb: " + STR$(ISAM.LASTREC(A2))

    ISAM CLOSE
END SUB
