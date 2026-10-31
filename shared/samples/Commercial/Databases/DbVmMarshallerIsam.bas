' FILE: Databases/DbVmMarshallerIsam.bas
' =====================================================================
' Test end-to-end del marshaller ISAM sul backend VM/C (gmb_vm_host):
' ISAM OPEN (pop strutturale + create-on-demand come C#), APPEND via UDT
' (dict -> CSV con sid cache), SEEK, CREATE INDEX, VALIDATE.
' Output: work/ (mai in root). Nessuna fixture richiesta.
' =====================================================================
IMPORT ISAM
IMPORT IO

TYPE Item
  ID AS LONG
  Name AS STRING * 32
  Price AS DOUBLE
END TYPE

SUB Main()
    DIM g_work AS STRING
    g_work = PROJECTFOLDER() + "/work"
    IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    ON ERROR RESUME NEXT
    KILL g_work + "/vm_isam.isdb"
    ON ERROR GOTO 0

    DIM u AS Item
    u.ID = 3
    u.Name = "Gizmo"
    u.Price = 99.99

    ISAM OPEN g_work + "/vm_isam.isdb" OF Item ALIAS I INDEXED BY ID
    ISAM APPEND I, u
    ISAM SEEK I, "Gizmo"
    ISAM CREATE INDEX "BYNAME" ON I KEY ("Name") LEN 32
    ISAM VALIDATE I
    ISAM CLOSE I
    PRINT "VM-ISAM-MARSHALLER-OK"
END SUB