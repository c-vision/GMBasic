' Sample avanzato DBF — SELECT + JOIN WITH multi-tabella (4 backend).
'
' Dimostra:
'   1. CREATE 3 tabelle correlate: clienti, ordini, prodotti
'   2. APPEND RECORD su ciascuna
'   3. USE + ALIAS per aprire piu' aree
'   4. SELECT per commutare l'area attiva prima di JOIN
'   5. JOIN WITH per incrociare tabelle su condizioni qualificate
'   6. .LASTREC() per contare i risultati dei JOIN
'
' Schema:
'   clienti(C):     ID(5), NOME(10)     → C1:ANNA, C2:BOB
'   ordini(O):      ID(5), IDCLI(5), COD(5) → O1:C1:PA, O2:C2:PB, O3:C1:PB
'   prodotti(P):    COD(5), PREZZO(N)    → PA:10.5, PB:20.0
'
' Output atteso (4 linee):
'   3  (C×O su ID=IDCLI: C1-O1, C1-O3, C2-O2)
'   3  (O×P su COD: O1-PA, O2-PB, O3-PB)
'   2  (C×P su ID=COD: nessun match, 0; ma dopo SELECT C e JOIN P×C su ID=COD = 0)
'   3  (O×C su IDCLI=ID: stesso 3, ma con inversione primary/secondary)
IMPORT DBF

TYPE TCLI
    ID AS STRING * 5
    NOME AS STRING * 10
END TYPE
TYPE TORD
    ID AS STRING * 5
    IDCLI AS STRING * 5
    COD AS STRING * 5
END TYPE
TYPE TPROD
    COD AS STRING * 5
    PREZZO AS DOUBLE
END TYPE

SUB Main()
    ' ── Setup 3 tabelle ──
    DBF CREATE "/tmp/gmb_adv_c.dbf" FROM TYPE TCLI
    DBF APPEND RECORD {ID: "C1", NOME: "ANNA"}
    DBF APPEND RECORD {ID: "C2", NOME: "BOB"}
    DBF CLOSE

    DBF CREATE "/tmp/gmb_adv_o.dbf" FROM TYPE TORD
    DBF APPEND RECORD {ID: "O1", IDCLI: "C1", COD: "PA"}
    DBF APPEND RECORD {ID: "O2", IDCLI: "C2", COD: "PB"}
    DBF APPEND RECORD {ID: "O3", IDCLI: "C1", COD: "PB"}
    DBF CLOSE

    DBF CREATE "/tmp/gmb_adv_p.dbf" FROM TYPE TPROD
    DBF APPEND RECORD {COD: "PA", PREZZO: 10.5}
    DBF APPEND RECORD {COD: "PB", PREZZO: 20.0}
    DBF CLOSE

    ' ── Passo 1: SELECT C + JOIN C×O (ID = IDCLI) ──
    ' Apri C poi O: current = O (ultimo USE)
    DBF USE "/tmp/gmb_adv_c.dbf" ALIAS C
    DBF USE "/tmp/gmb_adv_o.dbf" ALIAS O
    ' SELECT C: rende current = C (clienti come primary)
    DBF SELECT C
    DBF JOIN WITH O TO "/tmp/gmb_adv_co.dbf" FOR C.ID = O.IDCLI
    DBF USE "/tmp/gmb_adv_co.dbf" ALIAS CO
    PRINT DBF.LASTREC()        ' 3: C1-O1, C1-O3, C2-O2
    DBF CLOSE

    ' ── Passo 2: SELECT O + JOIN O×P (COD) ──
    DBF USE "/tmp/gmb_adv_o.dbf" ALIAS O
    DBF USE "/tmp/gmb_adv_p.dbf" ALIAS P ' current=P, poi SELECT O
    DBF SELECT O
    DBF JOIN WITH P TO "/tmp/gmb_adv_op.dbf" FOR O.COD = P.COD
    DBF USE "/tmp/gmb_adv_op.dbf" ALIAS OP
    PRINT DBF.LASTREC()        ' 3: O1-PA, O2-PB, O3-PB
    DBF CLOSE

    ' ── Passo 3: SELECT C + JOIN C×P (ID=COD → 0 match) ──
    ' ID in C = C1,C2; COD in P = PA,PB → nessun match
    DBF USE "/tmp/gmb_adv_c.dbf" ALIAS C
    DBF USE "/tmp/gmb_adv_p.dbf" ALIAS P
    DBF SELECT C
    DBF JOIN WITH P TO "/tmp/gmb_adv_cp.dbf" FOR C.ID = P.COD
    DBF USE "/tmp/gmb_adv_cp.dbf" ALIAS CP
    PRINT DBF.LASTREC()        ' 0: nessun match (C1/C2 vs PA/PB)
    DBF CLOSE

    ' ── Passo 4: JOIN O×C (SENZA SELECT — current = O per ultimo USE) ──
    ' Dimostra che USE determina l'area attiva di default
    DBF USE "/tmp/gmb_adv_o.dbf" ALIAS O
    DBF USE "/tmp/gmb_adv_c.dbf" ALIAS C ' current = C (ultimo)
    ' ORA SELECT O per usare ordini come primary
    DBF SELECT O
    DBF JOIN WITH C TO "/tmp/gmb_adv_oc.dbf" FOR O.IDCLI = C.ID
    DBF USE "/tmp/gmb_adv_oc.dbf" ALIAS OC
    PRINT DBF.LASTREC()       ' 3: O1-C1, O2-C2, O3-C1
    DBF CLOSE
END SUB
