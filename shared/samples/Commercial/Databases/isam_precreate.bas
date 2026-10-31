TYPE Cliente
    ID AS INTEGER
    Nome AS STRING * 50
    Citta AS STRING * 30
    Saldo AS DOUBLE
    Attivo AS BOOLEAN
END TYPE

TYPE Ordine
    ID_Ordine AS INTEGER
    ClienteID AS INTEGER
    DataOrdine AS STRING * 10
    Importo AS DOUBLE
END TYPE

IMPORT ISAM WITH ALIAS

SUB Main()
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    ISAM OPEN g_work + "/clienti.isdb" OF Cliente ALIAS C INDEXED BY ID, Nome, Citta
ISAM OPEN g_work + "/ordini.isdb" OF Ordine ALIAS O INDEXED BY ID_Ordine, DataOrdine
END SUB
