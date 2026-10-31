' FILE: samples/modules/FolderResolution/libs/helpers/Deep.bas
' Caricato per transitività: lo importa chain/Chain.bas.
NAMESPACE DEMO_DEEP

PUBLIC FUNCTION DeepOrigin() AS STRING
    RETURN "libs/helpers (importato da DEMO_CHAIN)"
END FUNCTION
