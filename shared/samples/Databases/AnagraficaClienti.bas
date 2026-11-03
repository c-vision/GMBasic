' FILE: Databases/AnagraficaClienti.bas
' SCENARIO DIDATTICO — anagrafica clienti su DBF.
'
' Un ufficio commerciale registra i clienti con città e saldo, poi interroga l'archivio:
' cerca un cliente per nome e ne legge la città (LOOKUP), verifica che il filtro attivo
' sia vuoto (DBFILTER) e conta i record. Mostra il ciclo CRUD essenziale su una tabella.
'
' Domini: DBF + IO + base. Il database vive in work/ (cancellato a fine scenario).
IMPORT DBF
IMPORT IO

TYPE TCliente
    Nome AS STRING * 20
    Citta AS STRING * 15
    Saldo AS DOUBLE
END TYPE

SUB Main()
    MKDIR "work"
    KILL "work/clienti.dbf"

    ' 1) Creazione della tabella e inserimento.
    DBF CREATE "work/clienti.dbf" FROM TYPE TCliente
    DBF USE "work/clienti.dbf" ALIAS C EXCLUSIVE
    DIM r AS TCliente
    r.Nome = "Anna" : r.Citta = "Roma"   : r.Saldo = 1200
    DBF APPEND RECORD r
    r.Nome = "Bob"  : r.Citta = "Milano" : r.Saldo = 800
    DBF APPEND RECORD r
    r.Nome = "Carla": r.Citta = "Torino" : r.Saldo = 450
    DBF APPEND RECORD r
    PRINT "clienti: "; DBF.LASTREC()

    ' 2) Filtro attivo (stringa vuota = nessun filtro) e lookup per nome.
    DBF GO TOP
    PRINT "filtro vuoto: "; (LEN(DBF.DBFILTER("C")) = 0)
    PRINT "citta di Bob: "; TRIM(DBF.LOOKUP("Citta", "Bob", "Nome", "C"))
    PRINT "citta di Carla: "; TRIM(DBF.LOOKUP("Citta", "Carla", "Nome", "C"))

    ' 3) Un cliente inesistente non deve rompere la ricerca.
    PRINT "sconosciuto: ["; DBF.LOOKUP("Citta", "Zoe", "Nome", "C"); "]"

    DBF CLOSE DATABASES
    KILL "work/clienti.dbf"
END SUB
