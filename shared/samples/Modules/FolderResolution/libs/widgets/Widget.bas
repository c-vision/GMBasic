' FILE: samples/modules/FolderResolution/libs/widgets/Widget.bas
' Trovato da `compiler.searchFolders`: "{project}/libs/*" (glob su una sottocartella).
NAMESPACE DEMO_WIDGET

PUBLIC FUNCTION WidgetOrigin() AS STRING
    RETURN "libs/widgets (glob di searchFolders)"
END FUNCTION
