' FILE: samples/modules/FolderResolution/include/LocalInclude.bas
' Trovato da `compiler.searchFolders`: "include" (progetto, poi risalita).
NAMESPACE DEMO_INCLUDE

PUBLIC FUNCTION IncludeOrigin() AS STRING
    RETURN "include/ locale del progetto"
END FUNCTION
