' FILE: samples/Classes/ClassesInterfacesNamespace/Contracts.inc
' =================================================================================================
'   Contratti (INTERFACE) esportati da un NAMESPACE — fix Z2
' =================================================================================================
' Prima del fix Z2 le INTERFACE dichiarate dentro un NAMESPACE NON venivano esportate:
' erano visibili solo nello stesso file/modulo. Ora una `PUBLIC INTERFACE` in un NAMESPACE
' è importabile con `IMPORT` e implementabile da altre classi (`IMPLEMENTS`).
'
' Il NAMESPACE termina a fine file (nessun END NAMESPACE).
' =================================================================================================

NAMESPACE Shapes

' Contratto: un oggetto che sa restituire la sua area e un etichetta.
PUBLIC INTERFACE ICalcolabile
    PUBLIC FUNCTION Area() AS DOUBLE
    PUBLIC FUNCTION Etichetta() AS STRING
END INTERFACE

' Contratto: un oggetto disegnabile su console.
PUBLIC INTERFACE IDisegnabile
    PUBLIC SUB Disegna()
END INTERFACE
