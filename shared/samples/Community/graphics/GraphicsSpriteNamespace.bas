' FILE: Graphics/GraphicsSpriteNamespace.bas
' ======================================================================================================================
' GUIDA TEORICA: NAMESPACE VIRTUALE CON IMPORT + ALIAS (SPRITES)
' ======================================================================================================================
' Questo programma dimostra come creare un "namespace virtuale" — un alias locale che rende
' più breve e leggibile l'accesso a un dominio esistente — usando la direttiva IMPORT.
'
' 1. NAMESPACE CANONICO E ALIAS
'    - Il dominio degli sprite vive nel namespace `GRAPHICS` (insieme a tutto il resto della grafica).
'    - `IMPORT GRAPHICS AS SPRITES` importa TUTTI i simboli PUBLIC di GRAPHICS, ma li rende
'      accessibili (in modalità qualificata) tramite l'alias locale `SPRITES`.
'    - L'alias è valido solo nel file corrente (modulo): non "inquina" gli altri moduli.
'
' 2. NAMESPACE VIRTUALE
'    - Dal punto di vista del programmatore, `SPRITES` si comporta come un namespace a sé stante:
'      raggruppa logicamente SOLO le operazioni sugli sprite, senza dover creare un nuovo dominio
'      nel linguaggio. L'alias non introduce una nuova implementazione: punta alla stessa GRAPHICS.
'    - In questo esempio usiamo l'alias per la modalità qualificata (`SPRITES.XXX ...`) e i comandi
'      legacy (`SPRITE LOAD`, `SPRITE SET POSITION`) per il resto.
'
' 3. IMPORT SELETTIVO
'    - `IMPORT GRAPHICS (SPRITE, SYNC)` importa solo i membri elencati, senza tutto il resto del dominio.
'    - La combinazione `IMPORT GRAPHICS (SPRITE, SYNC) AS SPRITES` crea un namespace virtuale
'      selettivo: espone solo gli sprite, con un nome corto.
'
' ======================================================================================================================

' --- Variante A: namespace virtuale "completo" (tutto GRAPHICS sotto SPRITES) ---
IMPORT GRAPHICS AS SPRITES

' --- Variante B: namespace virtuale selettivo (solo i membri sprite) ---
' IMPORT GRAPHICS (SPRITE, SYNC) AS SPRITES

SUB Main()
  ' Attiva la modalità grafica (640x480, 16 colori).
  SCREEN 12
  FONT "font.ttf", 12

  ' Carica uno sprite usando il comando legacy (il dominio è GRAPHICS, ma l'alias
  ' non cambia i comandi legacy: `SPRITE` resta una keyword del linguaggio).
  DIM Spr AS LONG
  SPRITE LOAD "player.png", Spr
  IF Spr = 0 THEN
    PRINT "Errore: impossibile caricare player.png"
    END
  END IF

  ' Posiziona lo sprite con i comandi legacy multi-parola.
  SPRITE SET POSITION Spr, 100, 80
  SPRITE ON Spr

  ' Sincronizza (rendering in batch sulla GPU).
  SYNC

  PRINT "Sprite caricato e posizionato via namespace virtuale SPRITES."
  WAITKEY
END SUB
