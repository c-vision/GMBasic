' FILE: samples/modules/FolderResolution/chain/Chain.bas
' Il resolver deve seguire ANCHE questo IMPORT: la entry importa solo DEMO_CHAIN.
IMPORT DEMO_DEEP

NAMESPACE DEMO_CHAIN

PUBLIC FUNCTION ChainOrigin() AS STRING
    RETURN "DEMO_CHAIN -> " + DeepOrigin()
END FUNCTION
