' FILE: samples/modules/FolderResolution/AllFolders.bas
' =================================================================================================
'   O7 — RISOLUZIONE DEI MODULI: OGNI CARTELLA CHE IL COMPILATORE CONSULTA
' =================================================================================================
' Ogni IMPORT qui sotto arriva da una cartella DIVERSA. Il resolver (Orchestrator C# e driver
' nativo usano la stessa configurazione) deve trovarli tutti, anche quelli fuori dalla radice
' del programma, e deve seguire gli IMPORT dei moduli caricati (DEMO_CHAIN -> DEMO_DEEP).
'
'   DEMO_NESTED    deep/nested/Nested.bas          scansione RICORSIVA della radice
'   DEMO_CHAIN     chain/Chain.bas                 idem, e importa DEMO_DEEP (transitivo)
'   DEMO_DEEP      libs/helpers/Deep.bas           searchFolders `{project}/libs/*`
'   DEMO_INCLUDE   include/LocalInclude.bas        searchFolders `include`
'   DEMO_CONTROL   Controls/LocalControl.bas       searchFolders `Controls`
'   DEMO_WIDGET    libs/widgets/Widget.bas         searchFolders `{project}/libs/*`
'   DEMO_SHARED    ../shared/SharedModule.bas      importFolders di app.settings.json
'   DEMO_PINNED    ../pinned/PinnedOnly.bas        importFiles di app.settings.json
' =================================================================================================

IMPORT DEMO_INCLUDE
IMPORT DEMO_CONTROL
IMPORT DEMO_NESTED
IMPORT DEMO_SHARED
IMPORT DEMO_PINNED
IMPORT DEMO_WIDGET
IMPORT DEMO_CHAIN

SUB Main
    PRINT "include/              -> "; IncludeOrigin()
    PRINT "Controls/             -> "; ControlOrigin()
    PRINT "radice (ricorsiva)    -> "; NestedOrigin()
    PRINT "libs/widgets (glob)   -> "; WidgetOrigin()
    PRINT "importFolders         -> "; SharedOrigin()
    PRINT "importFiles           -> "; PinnedOrigin()
    PRINT "IMPORT transitivo     -> "; ChainOrigin()
    PRINT "chiamata dalla entry  -> "; DeepOrigin()
END SUB
