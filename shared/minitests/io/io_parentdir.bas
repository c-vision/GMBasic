' MiniTest IO/FILES — IO.PARENTDIR: semantica canonica (decisione "B").
' Copre: IO.PARENTDIR.
'
' Semantica canonica = System.IO.Path.GetDirectoryName (oracolo verificato con dotnet fsi):
'   "/a/b/c"→"/a/b"  "/a/b/"→"/a/b"  "/a/b//"→"/a/b"  "/a/b/c/"→"/a/b/c"
'   "/a"→"/"  "/"→""  "a/b"→"a"  "a"→""  "/a//b"→"/a"  "//a"→"/"
' Regola: se il path termina con un separatore, l'ultimo segmento è una directory → il risultato
' è il path senza i separatori finali; altrimenti tutto ciò che precede l'ultimo separatore.
'
' Allineati TUTTI i backend: IL/PCode lo facevano già; il runtime condiviso ora implementa la
' stessa regola (gmb_fs_dir_name per C99/C99Native, gmb_path_parent per VMC) — prima c'erano
' TRE semantiche diverse (gmb_fs_dir_name "ultimo slash", gmb_path_parent POSIX, Path.GetDirectoryName).
' Solo path ASSOLUTI o relativi SENZA dipendenza dalla CWD (GetDirectoryName non consulta la CWD).
'
' ORACOLO (il MiniTest e' l'oracolo): output atteso, righe unite da '|'.
' EXPECT: /a/b|/a/b|/a/b|/a/b/c|/|a|/a|/
SUB Main()
    PRINT IO.PARENTDIR("/a/b/c")
    PRINT IO.PARENTDIR("/a/b/")
    PRINT IO.PARENTDIR("/a/b//")
    PRINT IO.PARENTDIR("/a/b/c/")
    PRINT IO.PARENTDIR("/a")
    PRINT IO.PARENTDIR("/")
    PRINT IO.PARENTDIR("a/b")
    PRINT IO.PARENTDIR("senza-slash")
    PRINT IO.PARENTDIR("/a//b")
    PRINT IO.PARENTDIR("//a")
END SUB
