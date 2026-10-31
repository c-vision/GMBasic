' MiniTest GREP — documenti office (docx/rtf), 7z e archivi annidati
' EXPECT: docx= 1  rtf= 1  7z= 1  nest= 1 
IMPORT GREP
SUB Main()
    DIM p AS STRING
    p = PROJECTFOLDER()
    PRINT "docx="; GREP.COUNT("needle", p + "/doc.docx");
    PRINT " rtf="; GREP.COUNT("needle_rtf", p + "/doc.rtf");
    PRINT " 7z="; GREP.COUNT("needle_7z", p + "/doc.7z");
    PRINT " nest="; GREP.COUNT("needle_inner", p + "/outer.zip")
END SUB
