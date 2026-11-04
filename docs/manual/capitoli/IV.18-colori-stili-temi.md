# IV.18 — Colori, stili e temi

## Cosa ottieni da questo capitolo

Tre livelli di aspetto: i 16 colori del terminale, la palette RGB della finestra, e i temi che
cambiano l'aspetto di tutti i controlli insieme. Sapere quale usare evita il classico programma
con un pulsante grigio in mezzo a un tema scuro.

## 1. Livello 1: i 16 colori ANSI/VGA

Nella console i colori sono indici 0-15, con la sintassi vista in IV.17:

```basic
COLOR 14, 1     ' giallo su blu scuro
COLOR 12        ' rosso
COLOR           ' default
```

Sono **indici**, non RGB: il terminale decide le tinte reali. Due terminali diversi mostrano lo
stesso `COLOR 12` con due rossi diversi, ed è corretto così.

## 2. Livello 2: la palette RGB

Quando serve il colore esatto si legge la palette della finestra:

```basic
DIM pal(2) AS INTEGER
pal = CONSOLE.GETPALETTECOLOR(1)
PRINT "GETPALETTECOLOR(1): r="; pal(0); " g="; pal(1); " b="; pal(2)
```

La funzione restituisce una tripletta RGB per l'indice richiesto: è il ponte fra il numero che
usi in `COLOR` e il colore che finisce sullo schermo. Il sample `Console/ConsolePalette.bas`
esercita la palette e chiude con `PALETTE test` / `DONE`.

## 2b. I nomi della palette a 256 colori

Oltre ai 16 indici ANSI, il terminale espone una palette di 256 colori e ognuno ha un nome. Le
costanti `%NOME` valgono **l'indice** di quel colore, quindi si usano ovunque si usi un numero di
colore:

```basic
COLOR %BLUE3            ' primo piano
COLOR %BLACK, %GRAY15   ' testo scuro su fondo chiaro
PRINT "indice di %AQUA: "; %AQUA
```

Il valore è un indice, non una tripletta RGB: la tinta reale la decide il terminale, come per i 16
colori del paragrafo precedente. I suffissi `_2`, `_3`, `_4` distinguono le tinte che nella palette
originale portano lo stesso nome ASCII: sono colori diversi, e la costante li tiene separati invece
di fonderli. La tabella seguente è la corrispondenza completa nome → indice.

| costante | indice | costante | indice | costante | indice | costante | indice |
|---|---|---|---|---|---|---|---|
| `%AQUA` | 14 | `%AQUAMARINE1` | 86 | `%AQUAMARINE1_2` | 122 | `%AQUAMARINE3` | 79 |
| `%BLACK` | 0 | `%BLUE` | 12 | `%BLUE1` | 21 | `%BLUE3` | 19 |
| `%BLUE3_2` | 20 | `%BLUE_VIOLET` | 57 | `%CADET_BLUE` | 72 | `%CADET_BLUE_2` | 73 |
| `%CHARTREUSE1` | 118 | `%CHARTREUSE2` | 82 | `%CHARTREUSE2_2` | 112 | `%CHARTREUSE3` | 70 |
| `%CHARTREUSE3_2` | 76 | `%CHARTREUSE4` | 64 | `%CORNFLOWER_BLUE` | 69 | `%CYAN1` | 51 |
| `%CYAN2` | 50 | `%CYAN3` | 43 | `%DARK_BLUE` | 18 | `%DARK_CYAN` | 36 |
| `%DARK_GREEN` | 22 | `%DARK_MAGENTA` | 90 | `%DARK_MAGENTA_2` | 91 | `%DARK_OLIVE_GREEN3` | 107 |
| `%DARK_OLIVE_GREEN3_2` | 113 | `%DARK_RED` | 52 | `%DARK_RED_2` | 88 | `%DARK_SEA_GREEN` | 108 |
| `%DARK_SEA_GREEN3` | 115 | `%DARK_SEA_GREEN4` | 65 | `%DARK_SEA_GREEN4_2` | 71 | `%DARK_SLATE_GRAY1` | 123 |
| `%DARK_SLATE_GRAY2` | 87 | `%DARK_SLATE_GRAY3` | 116 | `%DARK_TURQUOISE` | 44 | `%DARK_VIOLET` | 92 |
| `%DEEP_PINK4` | 53 | `%DEEP_PINK4_2` | 89 | `%DEEP_SKY_BLUE1` | 39 | `%DEEP_SKY_BLUE2` | 38 |
| `%DEEP_SKY_BLUE3` | 31 | `%DEEP_SKY_BLUE3_2` | 32 | `%DEEP_SKY_BLUE4` | 23 | `%DEEP_SKY_BLUE4_2` | 24 |
| `%DEEP_SKY_BLUE4_3` | 25 | `%DODGER_BLUE1` | 33 | `%DODGER_BLUE2` | 27 | `%DODGER_BLUE3` | 26 |
| `%FUCHSIA` | 13 | `%GRAY` | 8 | `%GRAY0` | 16 | `%GRAY11` | 235 |
| `%GRAY15` | 236 | `%GRAY19` | 237 | `%GRAY2` | 232 | `%GRAY23` | 238 |
| `%GRAY27` | 239 | `%GRAY3` | 233 | `%GRAY30` | 240 | `%GRAY35` | 241 |
| `%GRAY37` | 59 | `%GRAY39` | 242 | `%GRAY42` | 243 | `%GRAY46` | 244 |
| `%GRAY50` | 245 | `%GRAY53` | 102 | `%GRAY54` | 246 | `%GRAY58` | 247 |
| `%GRAY62` | 248 | `%GRAY66` | 249 | `%GRAY7` | 234 | `%GRAY70` | 250 |
| `%GRAY74` | 251 | `%GRAY78` | 252 | `%GRAY82` | 253 | `%GRAY85` | 254 |
| `%GRAY89` | 255 | `%GREEN` | 2 | `%GREEN1` | 46 | `%GREEN3` | 34 |
| `%GREEN3_2` | 40 | `%GREEN4` | 28 | `%LIGHT_GREEN` | 119 | `%LIGHT_GREEN_2` | 120 |
| `%LIGHT_PINK4` | 95 | `%LIGHT_SEA_GREEN` | 37 | `%LIGHT_SKY_BLUE3` | 109 | `%LIGHT_SKY_BLUE3_2` | 110 |
| `%LIGHT_SLATE_BLUE` | 105 | `%LIGHT_SLATE_GRAY` | 103 | `%LIME` | 10 | `%MAROON` | 1 |
| `%MEDIUM_PURPLE` | 104 | `%MEDIUM_PURPLE3` | 97 | `%MEDIUM_PURPLE3_2` | 98 | `%MEDIUM_PURPLE4` | 60 |
| `%MEDIUM_SPRING_GREEN` | 49 | `%MEDIUM_TURQUOISE` | 80 | `%NAVY` | 4 | `%NAVY_BLUE` | 17 |
| `%OLIVE` | 3 | `%ORANGE4` | 58 | `%ORANGE4_2` | 94 | `%PALE_GREEN1` | 121 |
| `%PALE_GREEN3` | 77 | `%PALE_GREEN3_2` | 114 | `%PALE_TURQUOISE4` | 66 | `%PLUM4` | 96 |
| `%PURPLE` | 5 | `%PURPLE1` | 93 | `%PURPLE3` | 56 | `%PURPLE4` | 54 |
| `%PURPLE4_2` | 55 | `%RED` | 9 | `%ROYAL_BLUE1` | 63 | `%SEA_GREEN1` | 84 |
| `%SEA_GREEN1_2` | 85 | `%SEA_GREEN2` | 83 | `%SEA_GREEN3` | 78 | `%SILVER` | 7 |
| `%SKY_BLUE1` | 117 | `%SKY_BLUE2` | 111 | `%SKY_BLUE3` | 74 | `%SLATE_BLUE1` | 99 |
| `%SLATE_BLUE3` | 61 | `%SLATE_BLUE3_2` | 62 | `%SPRING_GREEN1` | 48 | `%SPRING_GREEN2` | 42 |
| `%SPRING_GREEN2_2` | 47 | `%SPRING_GREEN3` | 35 | `%SPRING_GREEN3_2` | 41 | `%SPRING_GREEN4` | 29 |
| `%STEEL_BLUE` | 67 | `%STEEL_BLUE1` | 75 | `%STEEL_BLUE1_2` | 81 | `%STEEL_BLUE3` | 68 |
| `%TEAL` | 6 | `%TURQUOISE2` | 45 | `%TURQUOISE4` | 30 | `%WHEAT4` | 101 |
| `%WHITE` | 15 | `%YELLOW` | 11 | `%YELLOW4` | 100 | `%YELLOW4_2` | 106 |

I nomi base (`%BLACK`, `%WHITE`, `%RED`, `%GREEN`, `%BLUE`, `%YELLOW`, `%CYAN`, `%MAGENTA`) sono gli
stessi colori del primo livello, scritti per nome invece che per numero: mescolare le due forme in
un programma è lecito e non cambia il risultato. Per la palette **della finestra** in grafica resta
valido `CONSOLE.GETPALETTECOLOR`, che restituisce la tripletta RGB effettiva.

## 3. Livello 3: i temi

Nella grafica l'aspetto non si imposta controllo per controllo: si sceglie un tema e i controlli
lo seguono.

```basic
WINDOW THEME "light"
WINDOW SHOW
BUTTON 60, 50, 200, 44, "Cliccami" ON CLICK OnClick STYLE "rounded radius=8"
```

- `WINDOW THEME "light"` imposta il tema della finestra (chiaro/scuro, secondo il tema
  registrato);
- `STYLE "..."` è la personalizzazione **locale**: una stringa di proprietà separate da spazio
  (`rounded`, `radius=8`).

La precedenza è quella che serve: il tema dà la base, lo `STYLE` del singolo controllo la
corregge. Un controllo senza `STYLE` resta coerente col tema anche quando il tema cambia.

## 4. Tema, stile, proprietà: chi vince

| Livello | Ambito | Cambia con |
|---|---|---|
| tema | tutti i controlli della finestra | `WINDOW THEME` |
| stile | un controllo | `STYLE` alla creazione |
| proprietà | un aspetto specifico | `SET PROPERTY` o la proprietà del controllo |

Se un controllo sembra ignorare il tema, quasi sempre ha uno `STYLE` che fissa quel valore: è la
causa numero uno di "il tema non funziona".

## 5. Cosa non fare

- non mischiare 16 colori di console e RGB di finestra nello stesso schermo: sono due spazi di
  colore diversi;
- non fissare il colore di sfondo di ogni controllo a mano: al primo cambio di tema vanno
  corretti tutti;
- non usare il colore come unica informazione: chi non distingue rosso e verde deve poter
  leggere lo stato anche dal testo.

## 6. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| `COLOR 20` non dà il colore atteso | indice fuori da 0-15 | usa la palette RGB per tinte esatte |
| il tema non si applica a un controllo | `STYLE` locale che fissa il colore | togli la proprietà dal controllo |
| testo illeggibile su sfondo chiaro | colore del testo fisso | lascia che il tema scelga il contrasto |
| `GETPALETTECOLOR` restituisce zeri | nessuna finestra grafica attiva | chiamala dopo `WINDOW SHOW` |

## 7. Prova a farlo

1. In console, stampa la stessa riga con `COLOR 7`, `COLOR 15, 1` e `COLOR 0, 7`: guarda il
   contrasto, non la tinta.
2. In grafica, crea due pulsanti, uno con `STYLE` e uno senza, e cambia `WINDOW THEME`: vedrai
   quale dei due segue il tema.

## 8. Dove andare adesso

- **IV.17** — i comandi della console.
- **IV.19** — finestre ed eventi.
- **IV.20** — i controlli che ricevono stile e tema.



<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Palette-System

*Fonte: `Palette-System`*


## Panoramica

## 21 Palette Predefinite
| # | Nome | Origine |
|---|------|---------|
| 1 | Console | Palette di default per ScreenMode.Text |
| 2 | Solarized | Da Console/Program.cs |
| 3 | Dracula | Da Console/Program.cs |
| 4 | Nord | Da Console/Program.cs |
| 5 | Tokyo Night | Da Console/Program.cs |
| 6 | Gruvbox Dark | Curata (terminalcolors.com) |
| 7 | Catppuccin Mocha | Curata |
| 8 | One Half Dark | Curata |
| 9 | Ayu Dark | Curata |
| 10 | Gotham | Curata |
| 11 | Tomorrow Night | Curata |
| 12 | Nightfox | Curata |
| 13 | Everforest Dark | Curata |
| 14 | GitHub Dark | Curata |
| 15 | Kanagawa Wave | Curata |
| 16 | Rosé Pine | Curata |
| 17 | Nightfly | Curata |
| 18 | Cobalt2 | Curata |
| 19 | Jellybeans | Curata |
| 20 | Iceberg Dark | Curata |
| 21 | Srcery | Curata |

## Comandi

### `PALETTE` (console di testo — dal fork GMBasicAI)
| Sintassi | Descrizione |
|----------|-------------|
| `PALETTE idx, r, g, b` | Modifica il colore `idx` con i componenti RGB |
| `PALETTE USE "nome"` | Attiva una palette registrata per nome |
| `PALETTE DEFINE "nome", array()` | Definisce una nuova palette da array packed 0xRRGGBB |
| `PALETTE REPLACE "nome", array()` | Sostituisce una palette esistente |
| `PALETTE SAVE "file.json"` | Salva la palette attiva su file JSON |
| `PALETTE LOAD "file.json"` | Carica una palette da file JSON |

### `PALETTE` (GRAPHICS) `CPALETTE` (CONSOLE) — dal fork GM-Basic

## Truecolor ANSI

## Architettura del Codice

### File chiave GMBasic (unificato)
| File | Ruolo |
|------|-------|
| `GMBasicSharp/common/CommonLib/Screens/ScreenConfig.cs` | `NamedPalette`, 21 palette factory methods |
| `GMBasicSharp/common/CommonLib/Screens/PaletteManager.cs` | Registro globale, `RegisterPalette()`/`GetPalette()` |
| `GMBasicSharp/common/Ast/Graphics.cs` | AST: `PaletteStmt`, `PaletteUseStmt`, `PaletteDefineStmt`, etc. |
| `GMBasicSharp/common/Parser/Parser.Stmt.Console.cs` | `ParsePaletteStatement` (console) e `ParseCPaletteStatement` |
| `GMBasicSharp/common/Parser/Parser.Stmt.Graphics.cs` | `ParsePaletteStatement` con sub-comandi (graphics) |
| `GMBasicSharp/common/StdLib/StdLib.Graphics.cs` | Registrazione comandi + keyword |
| `GMBasicSharp/common/Compiler/Compiler.Graphics.cs` | Compilazione bytecode (chunk A1) |
| `GMBasicSharp/common/Linker/*.cs` | Binding/rewrite/resolve per i nuovi AST |
| `GMBasicSharp/common/CommonLib/Consoles/StandardConsole.cs` | Truecolor ANSI, `SetActivePalette` |
| `GMBasicSharp/common/CommonGraphics/Consoles/SdlConsole.cs` | `SetActivePalette` stub (SdlConsole.IConsole.cs) |

## Aggiunte al Lexer

## Note

### Palette-Colore-Guida-Utente

*Fonte: `Palette-Colore-Guida-Utente`*


## Concetto

## Comandi base

## Uso

COLOR 7          ' colore testo = indice 7 della palette
PALETTE SET 7, RGB(255, 200, 0)   ' indice 7 = ambra
PRINT "Testo"


## Palette nominate (2026-10-21)

## Convenzioni

### STYLE-Guida-Utente

*Fonte: `STYLE-Guida-Utente`*


## Sintassi

BUTTON 4, 4, 20, 6, "OK" STYLE "primary rounded radius=10"
PANEL 2, 2, 24, 6, "SEZIONE" STYLE "flat border=2,#334455"


DIM card AS STRING
card = "rounded radius=8 softshadow bg=#202833 fg=#E6EDF3"
PANEL 2, 2, 30, 8, "DETTAGLI" STYLE card


## Token supportati
| token | effetto |
|---|---|
| `primary` | sfondo = colore d'accento del tema, testo bianco |
| `flat` | forma piatta (radius 0, senza rilievo) |
| `rounded` | angoli arrotondati (radius di default 6) |
| `radius=N` | raggio di arrotondamento in pixel |
| `shadow` / `shadow=dx,dy` | ombra netta (offset opzionale) |
| `softshadow` | ombra morbida |
| `noshadow` | rimuove l'ombra |
| `raised` / `sunken` | rilievo positivo / negativo |
| `bevel` / `bevel=N` | bisello (profondità opzionale) |
| `emboss` / `embossed` | effetto incassato |
| `noemboss` | rimuove l'emboss |
| `gradient=a,b` | gradiente verticale fra due colori |
| `nogradient` | rimuove il gradiente |
| `bg=#RRGGBB` | colore di sfondo |
| `hover=#RRGGBB` | sfondo al passaggio del mouse |
| `press=#RRGGBB` | sfondo premuto |
| `disabled=#RRGGBB` | sfondo disabilitato |
| `fg=#RRGGBB` / `text=#RRGGBB` | colore del testo |
| `border` / `border=w` / `border=w,#RRGGBB` | bordo (spessore e colore) |
| `head=#RRGGBB` / `headfg=#RRGGBB` | header di tabelle/liste |
| `rowh=N` / `pad=N` | altezza riga / padding interno |
| `sel=#RRGGBB` / `selfg=#RRGGBB` | selezione (sfondo/testo) |
| `today=#RRGGBB` / `todayfg=#RRGGBB` | giorno corrente nei calendari |
| `glow` / `glow=#RRGGBB` / `noglow` | alone luminoso |
| `font=percorso[,size]` | font del controllo |

## Esempi

BUTTON 4, 4, 20, 6, "OK"     STYLE "primary rounded radius=10"
BUTTON 4,14, 20, 6, "ANNULLA" STYLE "flat border=1,#888888 fg=#DDDDDD"
PANEL  2, 2, 52, 10, "FILTRI" STYLE "rounded radius=8 softshadow bg=#1E2430"


## Disponibilità per backend (misurata)
| backend | `BUTTON … STYLE` | `PANEL` |
|---|---|---|
| IL | ✅ `style=ok` | ✅ `panel=ok` |
| P-Code | ✅ `style=ok` | ✅ `panel=ok` |
| C99Native | ✅ `style=ok` | ✅ `panel=ok` |
| VM/C | ✅ `style=ok` | ✅ `panel=ok` |

## Riferimenti

### STYLE-primary-Guida-Utente

*Fonte: `STYLE-primary-Guida-Utente`*


## Sintassi

<Controllo> ... STYLE "primary [altri token]"


BUTTON 48, 192, 208, 48, "Reports" STYLE "primary"
BUTTON 48, 320, 208, 48, "Save"    STYLE "primary rounded radius=10"


## Effetto
| proprietà | valore |
|---|---|
| sfondo (normale/hover/premuto) | accento del tema (soft: ambra `#F6A800`, neon: viola `#8B5CF6`, glass: ciano `#4FC3F7`) |
| testo | bianco |
| forma/raggio | invariati dal tema, sovrascrivibili con altri token (`rounded`, `radius=N`) |

## Backend
| backend | stato |
|---|---|
| IL | ✅ |
| PCodeVM | ✅ |
| VM/C | ✅ |
| C99Native | ✅ |

### THEME-Guida-Utente

*Fonte: `THEME-Guida-Utente`*


## Sintassi

IMPORT WINDOWS
WINDOW THEME "soft"        ' forma effettiva (applica il tema)
SET THEME TO "soft"        ' accettata ma NO-OP sul runtime nativo (compatibilità)


## Temi disponibili (13 famiglie)
| tema | alias accettati |
|---|---|
| `midnight` | — |
| `nord` | — |
| `dracula` | — |
| `qbasic` | `dos` |
| `monokai` | — |
| `solarized` | `solarizeddark` |
| `retrogreen` | `green`, `matrix` |
| `amber` | — |
| `turbo` | *(default)* |
| `light` | `win`, `windows` |
| `soft` | `neumorph`, `neumorphic` |
| `neon` | `violet` |
| `glass` | `dashboard`, `navy` |

## Esempi

SET THEME TO "nord"
WINDOW THEME "soft"
BUTTON 4, 4, 20, 6, "OK" STYLE "primary rounded radius=10"


## Disponibilità per backend
| backend | `WINDOW THEME` (con `IMPORT WINDOWS`) |
|---|---|
| IL | ✅ `theme=ok` |
| P-Code | ✅ `theme=ok` |
| VM/C | ✅ `theme=ok` |
| C99Native | ✅ `theme=ok` |

printf 'IMPORT WINDOWS\nWINDOW THEME "soft"\nPRINT "theme=ok"\n' > /tmp/themeI.bas
# 4 backend → theme=ok (commit 578e7307f)


## Riferimenti

<!-- END SURFACE -->
