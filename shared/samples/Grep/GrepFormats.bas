' FILE: Grep/GrepFormats.bas
' GREP — formato trasparente: testo, .Z (compress/LZW), .docx, .rtf, .7z, zip annidato.
' Usa tutte e 3 le funzioni (COUNT/FILES/MATCHES) e verifica la coerenza dei conteggi.
IMPORT GREP

SUB Main()
    DIM fx AS STRING
    fx = PROJECTFOLDER() + "/../../Fixtures/grep"
    PRINT "=== GREP formati ==="
    PRINT "txt  = "; GREP.COUNT("needle", fx + "/text.txt")
    PRINT "z    = "; GREP.COUNT("needle", fx + "/data.txt.Z")
    PRINT "docx = "; GREP.COUNT("needle", fx + "/doc.docx")
    PRINT "rtf  = "; GREP.COUNT("needle_rtf", fx + "/doc.rtf")
    PRINT "7z   = "; GREP.COUNT("needle_7z", fx + "/doc.7z")
    PRINT "nest = "; GREP.COUNT("needle_inner", fx + "/outer.zip")

    DIM f() AS STRING
    f = GREP.FILES("needle", fx + "/outer.zip")
    PRINT "files = "; UBOUND(f) + 1

    DIM hit() AS STRING
    hit = GREP.MATCHES("needle_inner", fx + "/outer.zip")
    PRINT "first = "; hit[0]
    PRINT "coerenza = "; GREP.COUNT("needle_inner", fx + "/outer.zip") = UBOUND(hit) + 1
    PRINT "=== OK ==="
END SUB
