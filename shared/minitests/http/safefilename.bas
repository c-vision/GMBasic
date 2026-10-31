' EXPECT: passwd.png|x.bin|report finale.PDF|file|file
IMPORT HTTP
PRINT HTTP.SAFEFILENAME("../../etc/passwd.png")
PRINT HTTP.SAFEFILENAME("C:\temp\..\x.bin")
PRINT HTTP.SAFEFILENAME("report finale.PDF")
PRINT HTTP.SAFEFILENAME("///")
PRINT HTTP.SAFEFILENAME("")
