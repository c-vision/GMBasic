' MiniTest GREP — file .Z (compress/LZW): decompressione trasparente
' EXPECT: count= 2 
IMPORT GREP
SUB Main()
    PRINT "count="; GREP.COUNT("needle", PROJECTFOLDER() + "/data_z.txt.Z")
END SUB
