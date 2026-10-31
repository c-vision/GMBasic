' FILE: Databases/DbVmMarshallerSpeed.bas
' =====================================================================
' Test del marshaller SPEED sul backend VM/C: OPEN, USE, PUT (dict->CSV),
' GO TOP, SEEK. Output: work/.
' =====================================================================
IMPORT SPEED
IMPORT IO

TYPE Cliente
  ID AS LONG
  Nome AS STRING * 30
  Saldo AS DOUBLE
END TYPE

SUB Main()
    DIM g_work AS STRING
    g_work = PROJECTFOLDER() + "/work"
    IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    SPEED OPEN g_work + "/vm_speed" ALIAS MioDb
    SPEED USE "Clienti" FROM MioDb OF Cliente ALIAS Clienti INDEXED BY ID

    DIM c AS Cliente
    c.ID = 101
    c.Nome = "Mario"
    c.Saldo = 150.75
    SPEED PUT Clienti, c
    c.ID = 102
    c.Nome = "Giulia"
    c.Saldo = 2300.5
    SPEED PUT Clienti, c

    SPEED GO Clienti, TOP
    SPEED SEEK Clienti, 102
    PRINT "VM-SPEED-MARSHALLER-OK"
    ' NOTA: SPEED_USE non porta i tipi dei campi (solo nomi+sid) -> la creazione
    ' strutturale della tabella nel marshaller e' un gap noto; qui si verifica che
    ' i path PUT/GO/SEEK eseguano senza crash sul backend VM/C.
END SUB