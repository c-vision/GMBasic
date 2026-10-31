' FILE: Modules/MathUtils.inc
' ======================================================================================================================
' GUIDA TEORICA: DEFINIZIONE DI LIBRERIE E VISIBILITÀ (NAMESPACE)
' ======================================================================================================================
' Questo file funge da "Libreria" per i test di integrazione del sistema di moduli. Non contiene un entry point (Main),
' ma definisce simboli che verranno consumati da altri moduli tramite `IMPORT`.
'
' 1. NAMESPACE COME CONTENITORE LOGICO
'    - La direttiva `NAMESPACE MATHUTILS` raggruppa tutte le procedure definite in questo file sotto un unico
'      identificatore logico. Questo previene collisioni di nomi (es. una funzione `Add` in un'altra libreria).
'
' 2. VISIBILITÀ DEI MEMBRI (ENCAPSULATION)
'    - PUBLIC: Le funzioni `Add`, `Subtract`, `Multiply` e `PrintMessage` sono marcate `PUBLIC`.
'      Saranno visibili e invocabili da qualsiasi modulo che importi `MATHUTILS`.
'    - PRIVATE (Implicito): La funzione `InternalHelper` non ha modificatori di accesso. In GM-BASIC,
'      il default all'interno di un namespace è `PRIVATE` (visibile solo all'interno dello stesso namespace/modulo).
'      Tentare di chiamare `MATHUTILS.InternalHelper` da un altro file genererà un errore di compilazione.
'
' 3. RUOLO NEI TEST
'    - Questo file è la dipendenza per:
'      * `ImportAlias.bas` (test aliasing `AS M`)
'      * `ImportSelective.bas` (test import parziale `{Add, Multiply}`)
'      * `ImportPriority.bas` (test shadowing nomi locali vs importati)
' ======================================================================================================================

NAMESPACE MATHUTILS

' Funzione pubblica per testare gli import e l'aritmetica di base.
' Visibile esternamente come MATHUTILS.Add (o tramite alias).
PUBLIC FUNCTION Add(a AS DOUBLE, b AS DOUBLE) AS DOUBLE
    RETURN a + b
END FUNCTION

' Funzione pubblica per testare gli import.
PUBLIC FUNCTION Subtract(a AS DOUBLE, b AS DOUBLE) AS DOUBLE
    RETURN a - b
END FUNCTION

' Funzione pubblica aggiuntiva specificamente per i test di import selettivi (IMPORT ... (Multiply)).
PUBLIC FUNCTION Multiply(a AS DOUBLE, b AS DOUBLE) AS DOUBLE
    RETURN a * b
END FUNCTION

' Subroutine pubblica utilizzata per verificare la priorità di risoluzione dei nomi.
' Il test `ImportPriority.bas` definisce una SUB locale con lo stesso nome per verificare quale viene chiamata.
PUBLIC SUB PrintMessage(msg AS STRING)
    PRINT "Message from MATHUTILS:", msg
END SUB

' Funzione privata (senza keyword PUBLIC).
' Questa funzione NON deve essere visibile o importabile da altri moduli.
' Serve a verificare che l'incapsulamento del namespace funzioni correttamente.
FUNCTION InternalHelper() AS LONG
    RETURN 42
END FUNCTION

' ' VERIFICA DI COMPLETEZZA
' Questo modulo è passivo (una libreria). La verifica della sua correttezza avviene
' compilando ed eseguendo i moduli "client" (ImportAlias, ImportSelective, ecc.) che dipendono da esso.