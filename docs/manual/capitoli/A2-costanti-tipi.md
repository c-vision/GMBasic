# A2 — Costanti globali e tipi

> Generata da `tools/sh/manual_coverage.sh`. Costanti dal registry canonico; per ognuna il valore,\n> un uso reale preso dal corpus (file:riga) oppure, se il corpus non la usa, un esempio\n> sintetico dichiarato con il valore canonico.

## Indice delle costanti

| Costante | Valore |
|---|---|
| `%ABORT` | 3 |
| `%ABORTRETRYIGNORE` | 104 |
| `%ANSI` | 2 |
| `%AQUA` | 14 |
| `%AQUAMARINE1` | 86 |
| `%AQUAMARINE1_2` | 122 |
| `%AQUAMARINE3` | 79 |
| `%AUDIO` | 2 |
| `%AUTO` | 0 |
| `%BLACK` | 0 |
| `%BLUE` | 12 |
| `%BLUE1` | 21 |
| `%BLUE3` | 19 |
| `%BLUE3_2` | 20 |
| `%BLUE_VIOLET` | 57 |
| `%C64` | 1 |
| `%CADET_BLUE` | 72 |
| `%CADET_BLUE_2` | 73 |
| `%CANCEL` | 2 |
| `%CENTER` | 1 |
| `%CHARTREUSE1` | 118 |
| `%CHARTREUSE2` | 82 |
| `%CHARTREUSE2_2` | 112 |
| `%CHARTREUSE3` | 70 |
| `%CHARTREUSE3_2` | 76 |
| `%CHARTREUSE4` | 64 |
| `%CHAT` | 0 |
| `%CHCLOSED` | 0 |
| `%CHROMIUM` | 1 |
| `%CLOSED` | -1 |
| `%CORNFLOWER_BLUE` | 69 |
| `%CYAN1` | 51 |
| `%CYAN2` | 50 |
| `%CYAN3` | 43 |
| `%DARK_BLUE` | 18 |
| `%DARK_CYAN` | 36 |
| `%DARK_GREEN` | 22 |
| `%DARK_MAGENTA` | 90 |
| `%DARK_MAGENTA_2` | 91 |
| `%DARK_OLIVE_GREEN3` | 107 |
| `%DARK_OLIVE_GREEN3_2` | 113 |
| `%DARK_RED` | 52 |
| `%DARK_RED_2` | 88 |
| `%DARK_SEA_GREEN` | 108 |
| `%DARK_SEA_GREEN3` | 115 |
| `%DARK_SEA_GREEN4` | 65 |
| `%DARK_SEA_GREEN4_2` | 71 |
| `%DARK_SLATE_GRAY1` | 123 |
| `%DARK_SLATE_GRAY2` | 87 |
| `%DARK_SLATE_GRAY3` | 116 |
| `%DARK_TURQUOISE` | 44 |
| `%DARK_VIOLET` | 92 |
| `%DECIDE` | 6 |
| `%DEEP_PINK4` | 53 |
| `%DEEP_PINK4_2` | 89 |
| `%DEEP_SKY_BLUE1` | 39 |
| `%DEEP_SKY_BLUE2` | 38 |
| `%DEEP_SKY_BLUE3` | 31 |
| `%DEEP_SKY_BLUE3_2` | 32 |
| `%DEEP_SKY_BLUE4` | 23 |
| `%DEEP_SKY_BLUE4_2` | 24 |
| `%DEEP_SKY_BLUE4_3` | 25 |
| `%DODGER_BLUE1` | 33 |
| `%DODGER_BLUE2` | 27 |
| `%DODGER_BLUE3` | 26 |
| `%DOUBLE` | 2 |
| `%EMBEDDING` | 3 |
| `%FAST` | 1 |
| `%FILE_CHANGED` | 4 |
| `%FILE_CREATED` | 1 |
| `%FILE_DELETED` | 2 |
| `%FILE_RENAMED` | 8 |
| `%FUCHSIA` | 13 |
| `%GOVERNED` | 2 |
| `%GRAY` | 8 |
| `%GRAY0` | 16 |
| `%GRAY11` | 235 |
| `%GRAY15` | 236 |
| `%GRAY19` | 237 |
| `%GRAY2` | 232 |
| `%GRAY23` | 238 |
| `%GRAY27` | 239 |
| `%GRAY3` | 233 |
| `%GRAY30` | 240 |
| `%GRAY35` | 241 |
| `%GRAY37` | 59 |
| `%GRAY39` | 242 |
| `%GRAY42` | 243 |
| `%GRAY46` | 244 |
| `%GRAY50` | 245 |
| `%GRAY53` | 102 |
| `%GRAY54` | 246 |
| `%GRAY58` | 247 |
| `%GRAY62` | 248 |
| `%GRAY66` | 249 |
| `%GRAY7` | 234 |
| `%GRAY70` | 250 |
| `%GRAY74` | 251 |
| `%GRAY78` | 252 |
| `%GRAY82` | 253 |
| `%GRAY85` | 254 |
| `%GRAY89` | 255 |
| `%GREEN` | 2 |
| `%GREEN1` | 46 |
| `%GREEN3` | 34 |
| `%GREEN3_2` | 40 |
| `%GREEN4` | 28 |
| `%IGNORE` | 5 |
| `%JOYBUTTON2` | 32 |
| `%JOYDOWN` | 2 |
| `%JOYFIRE` | 16 |
| `%JOYLEFT` | 4 |
| `%JOYRIGHT` | 8 |
| `%JOYUP` | 1 |
| `%K_0` | 48 |
| `%K_1` | 49 |
| `%K_2` | 50 |
| `%K_3` | 51 |
| `%K_4` | 52 |
| `%K_5` | 53 |
| `%K_6` | 54 |
| `%K_7` | 55 |
| `%K_8` | 56 |
| `%K_9` | 57 |
| `%K_A` | 65 |
| `%K_ALT` | 1 |
| `%K_B` | 66 |
| `%K_BS` | 8 |
| `%K_C` | 67 |
| `%K_CTRL` | 4 |
| `%K_CTRL_BRK` | -1 |
| `%K_CTRL_C` | -1 |
| `%K_D` | 68 |
| `%K_DEL` | 46 |
| `%K_DOWN` | 40 |
| `%K_E` | 69 |
| `%K_END` | 35 |
| `%K_ENTER` | 13 |
| `%K_ESC` | 27 |
| `%K_F` | 70 |
| `%K_F1` | 112 |
| `%K_F10` | 121 |
| `%K_F11` | 122 |
| `%K_F12` | 123 |
| `%K_F2` | 113 |
| `%K_F3` | 114 |
| `%K_F4` | 115 |
| `%K_F5` | 116 |
| `%K_F6` | 117 |
| `%K_F7` | 118 |
| `%K_F8` | 119 |
| `%K_F9` | 120 |
| `%K_G` | 71 |
| `%K_H` | 72 |
| `%K_HOME` | 36 |
| `%K_I` | 73 |
| `%K_INS` | 45 |
| `%K_J` | 74 |
| `%K_K` | 75 |
| `%K_L` | 76 |
| `%K_LEFT` | 37 |
| `%K_M` | 77 |
| `%K_N` | 78 |
| `%K_N0` | 96 |
| `%K_N1` | 97 |
| `%K_N2` | 98 |
| `%K_N3` | 99 |
| `%K_N4` | 100 |
| `%K_N5` | 101 |
| `%K_N6` | 102 |
| `%K_N7` | 103 |
| `%K_N8` | 104 |
| `%K_N9` | 105 |
| `%K_NADD` | 107 |
| `%K_NDEC` | 110 |
| `%K_NDIV` | 111 |
| `%K_NMUL` | 106 |
| `%K_NSUB` | 109 |
| `%K_O` | 79 |
| `%K_P` | 80 |
| `%K_PAUSE` | 19 |
| `%K_PGDN` | 34 |
| `%K_PGUP` | 33 |
| `%K_PRTSC` | 44 |
| `%K_Q` | 81 |
| `%K_R` | 82 |
| `%K_RIGHT` | 39 |
| `%K_S` | 83 |
| `%K_SHIFT` | 2 |
| `%K_SHIFT_TAB` | -1 |
| `%K_SPC` | 32 |
| `%K_T` | 84 |
| `%K_TAB` | 9 |
| `%K_U` | 85 |
| `%K_UP` | 38 |
| `%K_V` | 86 |
| `%K_W` | 87 |
| `%K_X` | 88 |
| `%K_Y` | 89 |
| `%K_Z` | 90 |
| `%LEFT` | 0 |
| `%LIGHT_GREEN` | 119 |
| `%LIGHT_GREEN_2` | 120 |
| `%LIGHT_PINK4` | 95 |
| `%LIGHT_SEA_GREEN` | 37 |
| `%LIGHT_SKY_BLUE3` | 109 |
| `%LIGHT_SKY_BLUE3_2` | 110 |
| `%LIGHT_SLATE_BLUE` | 105 |
| `%LIGHT_SLATE_GRAY` | 103 |
| `%LIME` | 10 |
| `%MAROON` | 1 |
| `%MEDIUM_PURPLE` | 104 |
| `%MEDIUM_PURPLE3` | 97 |
| `%MEDIUM_PURPLE3_2` | 98 |
| `%MEDIUM_PURPLE4` | 60 |
| `%MEDIUM_SPRING_GREEN` | 49 |
| `%MEDIUM_TURQUOISE` | 80 |
| `%NAVY` | 4 |
| `%NAVY_BLUE` | 17 |
| `%NO` | 7 |
| `%NONE` | 0 |
| `%OBSCURA` | 2 |
| `%OCR` | 1 |
| `%OK` | 1 |
| `%OKCANCEL` | 101 |
| `%OLIVE` | 3 |
| `%ORANGE4` | 58 |
| `%ORANGE4_2` | 94 |
| `%PALE_GREEN1` | 121 |
| `%PALE_GREEN3` | 77 |
| `%PALE_GREEN3_2` | 114 |
| `%PALE_TURQUOISE4` | 66 |
| `%PLUM4` | 96 |
| `%PURPLE` | 5 |
| `%PURPLE1` | 93 |
| `%PURPLE3` | 56 |
| `%PURPLE4` | 54 |
| `%PURPLE4_2` | 55 |
| `%RECEIVED` | 1 |
| `%RED` | 9 |
| `%RERANKER` | 4 |
| `%RERANK_KEYWORD` | 1 |
| `%RERANK_OFF` | 0 |
| `%RERANK_ONNX` | 2 |
| `%RETRY` | 4 |
| `%RETRYCANCEL` | 105 |
| `%RIGHT` | 2 |
| `%ROUNDED` | 3 |
| `%ROYAL_BLUE1` | 63 |
| `%SDL` | 0 |
| `%SEA_GREEN1` | 84 |
| `%SEA_GREEN1_2` | 85 |
| `%SEA_GREEN2` | 83 |
| `%SEA_GREEN3` | 78 |
| `%SILVER` | 7 |
| `%SINGLE` | 1 |
| `%SKY_BLUE1` | 117 |
| `%SKY_BLUE2` | 111 |
| `%SKY_BLUE3` | 74 |
| `%SLATE_BLUE1` | 99 |
| `%SLATE_BLUE3` | 61 |
| `%SLATE_BLUE3_2` | 62 |
| `%SPRING_GREEN1` | 48 |
| `%SPRING_GREEN2` | 42 |
| `%SPRING_GREEN2_2` | 47 |
| `%SPRING_GREEN3` | 35 |
| `%SPRING_GREEN3_2` | 41 |
| `%SPRING_GREEN4` | 29 |
| `%STEEL_BLUE` | 67 |
| `%STEEL_BLUE1` | 75 |
| `%STEEL_BLUE1_2` | 81 |
| `%STEEL_BLUE3` | 68 |
| `%SYSTEM` | 3 |
| `%TEAL` | 6 |
| `%TTS` | 5 |
| `%TURQUOISE2` | 45 |
| `%TURQUOISE4` | 30 |
| `%WHEAT4` | 101 |
| `%WHITE` | 15 |
| `%YELLOW` | 11 |
| `%YELLOW4` | 100 |
| `%YELLOW4_2` | 106 |
| `%YES` | 6 |
| `%YESNO` | 102 |
| `%YESNOCANCEL` | 103 |

## Costanti: valore ed esempio

### `%ABORT` — valore `3`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:30`*

```basic
PRINT "  %ABORT"; TAB(24); %ABORT
```

### `%ABORTRETRYIGNORE` — valore `104`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:38`*

```basic
PRINT "  %ABORTRETRYIGNORE"; TAB(24); %ABORTRETRYIGNORE
```

### `%ANSI` — valore `2`

*uso reale in `shared/samples/graphics/ConsoleModeSelect.bas:3`*

```basic
' TEXTMODE — selezione della modalita' di console a runtime + costanti %SDL/%C64/%ANSI/%SYSTEM
```

### `%AQUA` — valore `14`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:50`*

```basic
cnt = cnt + 1 : cn(cnt) = "AQUA" : ci(cnt) = %AQUA
```

### `%AQUAMARINE1` — valore `86`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:11`*

```basic
' USO: COLOR ColorIndex("AQUAMARINE1") — oppure COLOR %AQUAMARINE1.
```

### `%AQUAMARINE1_2` — valore `122`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:158`*

```basic
cnt = cnt + 1 : cn(cnt) = "AQUAMARINE1_2" : ci(cnt) = %AQUAMARINE1_2
```

### `%AQUAMARINE3` — valore `79`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:115`*

```basic
cnt = cnt + 1 : cn(cnt) = "AQUAMARINE3" : ci(cnt) = %AQUAMARINE3
```

### `%AUDIO` — valore `2`

*uso reale in `shared/samples/Ai/AiEmbeddedRegisterRouting.bas:20`*

```basic
'    - %CHAT/%OCR/%AUDIO (0/1/2) sono costanti predefinite usabili ovunque l'argomento
```

### `%AUTO` — valore `0`

*uso reale in `shared/samples/AiLLM/AiAgentRunCua.bas:16`*

```basic
AI AGENT SET "agent1" BROWSER %AUTO
```

### `%BLACK` — valore `0`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:36`*

```basic
cnt = cnt + 1 : cn(cnt) = "BLACK" : ci(cnt) = %BLACK
```

### `%BLUE` — valore `12`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:48`*

```basic
cnt = cnt + 1 : cn(cnt) = "BLUE" : ci(cnt) = %BLUE
```

### `%BLUE1` — valore `21`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:57`*

```basic
cnt = cnt + 1 : cn(cnt) = "BLUE1" : ci(cnt) = %BLUE1
```

### `%BLUE3` — valore `19`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:55`*

```basic
cnt = cnt + 1 : cn(cnt) = "BLUE3" : ci(cnt) = %BLUE3
```

### `%BLUE3_2` — valore `20`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:56`*

```basic
cnt = cnt + 1 : cn(cnt) = "BLUE3_2" : ci(cnt) = %BLUE3_2
```

### `%BLUE_VIOLET` — valore `57`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:93`*

```basic
cnt = cnt + 1 : cn(cnt) = "BLUE_VIOLET" : ci(cnt) = %BLUE_VIOLET
```

### `%C64` — valore `1`

*uso reale in `shared/samples/graphics/ConsoleModeSelect.bas:3`*

```basic
' TEXTMODE — selezione della modalita' di console a runtime + costanti %SDL/%C64/%ANSI/%SYSTEM
```

### `%CADET_BLUE` — valore `72`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:108`*

```basic
cnt = cnt + 1 : cn(cnt) = "CADET_BLUE" : ci(cnt) = %CADET_BLUE
```

### `%CADET_BLUE_2` — valore `73`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:109`*

```basic
cnt = cnt + 1 : cn(cnt) = "CADET_BLUE_2" : ci(cnt) = %CADET_BLUE_2
```

### `%CANCEL` — valore `2`

*uso reale in `shared/samples/Console/ConsoleControls.bas:14`*

```basic
' - v = CONFIRM("msg?")          : conferma -> %OK/%CANCEL/0.
```

### `%CENTER` — valore `1`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:95`*

```basic
PRINT "  %CENTER"; TAB(24); %CENTER
```

### `%CHARTREUSE1` — valore `118`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:154`*

```basic
cnt = cnt + 1 : cn(cnt) = "CHARTREUSE1" : ci(cnt) = %CHARTREUSE1
```

### `%CHARTREUSE2` — valore `82`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:118`*

```basic
cnt = cnt + 1 : cn(cnt) = "CHARTREUSE2" : ci(cnt) = %CHARTREUSE2
```

### `%CHARTREUSE2_2` — valore `112`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:148`*

```basic
cnt = cnt + 1 : cn(cnt) = "CHARTREUSE2_2" : ci(cnt) = %CHARTREUSE2_2
```

### `%CHARTREUSE3` — valore `70`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:106`*

```basic
cnt = cnt + 1 : cn(cnt) = "CHARTREUSE3" : ci(cnt) = %CHARTREUSE3
```

### `%CHARTREUSE3_2` — valore `76`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:112`*

```basic
cnt = cnt + 1 : cn(cnt) = "CHARTREUSE3_2" : ci(cnt) = %CHARTREUSE3_2
```

### `%CHARTREUSE4` — valore `64`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:100`*

```basic
cnt = cnt + 1 : cn(cnt) = "CHARTREUSE4" : ci(cnt) = %CHARTREUSE4
```

### `%CHAT` — valore `0`

*uso reale in `shared/samples/AiLLM/Qwen3_5_27B_Claude_4_6_Opus_Distilled_MLX_6bit.bas:17`*

```basic
PRINT "Attivo: "; AI.ACTIVE(%CHAT)
```

### `%CHCLOSED` — valore `0`

*uso reale in `shared/samples/Concurrency/PCodeGpmChannelClose.bas:3`*

```basic
' quando il flag ok diventa %CHCLOSED (0) — il pattern di shutdown Go-idiomatico
```

### `%CHROMIUM` — valore `1`

*uso reale in `shared/samples/Ai/BrowserProfileStart.bas:8`*

```basic
PRINT "start="; BROWSER.START(%CHROMIUM, 9255, "demo-userX")
```

### `%CLOSED` — valore `-1`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:40`*

```basic
PRINT "  %CLOSED"; TAB(24); %CLOSED
```

### `%CORNFLOWER_BLUE` — valore `69`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:105`*

```basic
cnt = cnt + 1 : cn(cnt) = "CORNFLOWER_BLUE" : ci(cnt) = %CORNFLOWER_BLUE
```

### `%CYAN1` — valore `51`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:87`*

```basic
cnt = cnt + 1 : cn(cnt) = "CYAN1" : ci(cnt) = %CYAN1
```

### `%CYAN2` — valore `50`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:86`*

```basic
cnt = cnt + 1 : cn(cnt) = "CYAN2" : ci(cnt) = %CYAN2
```

### `%CYAN3` — valore `43`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:79`*

```basic
cnt = cnt + 1 : cn(cnt) = "CYAN3" : ci(cnt) = %CYAN3
```

### `%DARK_BLUE` — valore `18`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:54`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_BLUE" : ci(cnt) = %DARK_BLUE
```

### `%DARK_CYAN` — valore `36`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:72`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_CYAN" : ci(cnt) = %DARK_CYAN
```

### `%DARK_GREEN` — valore `22`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:58`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_GREEN" : ci(cnt) = %DARK_GREEN
```

### `%DARK_MAGENTA` — valore `90`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:126`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_MAGENTA" : ci(cnt) = %DARK_MAGENTA
```

### `%DARK_MAGENTA_2` — valore `91`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:127`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_MAGENTA_2" : ci(cnt) = %DARK_MAGENTA_2
```

### `%DARK_OLIVE_GREEN3` — valore `107`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:143`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_OLIVE_GREEN3" : ci(cnt) = %DARK_OLIVE_GREEN3
```

### `%DARK_OLIVE_GREEN3_2` — valore `113`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:149`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_OLIVE_GREEN3_2" : ci(cnt) = %DARK_OLIVE_GREEN3_2
```

### `%DARK_RED` — valore `52`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:88`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_RED" : ci(cnt) = %DARK_RED
```

### `%DARK_RED_2` — valore `88`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:124`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_RED_2" : ci(cnt) = %DARK_RED_2
```

### `%DARK_SEA_GREEN` — valore `108`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:144`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_SEA_GREEN" : ci(cnt) = %DARK_SEA_GREEN
```

### `%DARK_SEA_GREEN3` — valore `115`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:151`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_SEA_GREEN3" : ci(cnt) = %DARK_SEA_GREEN3
```

### `%DARK_SEA_GREEN4` — valore `65`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:101`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_SEA_GREEN4" : ci(cnt) = %DARK_SEA_GREEN4
```

### `%DARK_SEA_GREEN4_2` — valore `71`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:107`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_SEA_GREEN4_2" : ci(cnt) = %DARK_SEA_GREEN4_2
```

### `%DARK_SLATE_GRAY1` — valore `123`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:159`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_SLATE_GRAY1" : ci(cnt) = %DARK_SLATE_GRAY1
```

### `%DARK_SLATE_GRAY2` — valore `87`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:123`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_SLATE_GRAY2" : ci(cnt) = %DARK_SLATE_GRAY2
```

### `%DARK_SLATE_GRAY3` — valore `116`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:152`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_SLATE_GRAY3" : ci(cnt) = %DARK_SLATE_GRAY3
```

### `%DARK_TURQUOISE` — valore `44`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:80`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_TURQUOISE" : ci(cnt) = %DARK_TURQUOISE
```

### `%DARK_VIOLET` — valore `92`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:128`*

```basic
cnt = cnt + 1 : cn(cnt) = "DARK_VIOLET" : ci(cnt) = %DARK_VIOLET
```

### `%DECIDE` — valore `6`

*uso reale in `shared/samples/AiLLM/CLMLocal.bas:16`*

```basic
AI REGISTER "clm", %DECIDE, "clm", "{""head"":""~/ai/Models/CLM-v0.1-8B/clm_head.safetensors"",""encoder"":""~/ai/Models/Qwen3-8B""}"
```

### `%DEEP_PINK4` — valore `53`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:89`*

```basic
cnt = cnt + 1 : cn(cnt) = "DEEP_PINK4" : ci(cnt) = %DEEP_PINK4
```

### `%DEEP_PINK4_2` — valore `89`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:125`*

```basic
cnt = cnt + 1 : cn(cnt) = "DEEP_PINK4_2" : ci(cnt) = %DEEP_PINK4_2
```

### `%DEEP_SKY_BLUE1` — valore `39`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:75`*

```basic
cnt = cnt + 1 : cn(cnt) = "DEEP_SKY_BLUE1" : ci(cnt) = %DEEP_SKY_BLUE1
```

### `%DEEP_SKY_BLUE2` — valore `38`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:74`*

```basic
cnt = cnt + 1 : cn(cnt) = "DEEP_SKY_BLUE2" : ci(cnt) = %DEEP_SKY_BLUE2
```

### `%DEEP_SKY_BLUE3` — valore `31`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:67`*

```basic
cnt = cnt + 1 : cn(cnt) = "DEEP_SKY_BLUE3" : ci(cnt) = %DEEP_SKY_BLUE3
```

### `%DEEP_SKY_BLUE3_2` — valore `32`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:68`*

```basic
cnt = cnt + 1 : cn(cnt) = "DEEP_SKY_BLUE3_2" : ci(cnt) = %DEEP_SKY_BLUE3_2
```

### `%DEEP_SKY_BLUE4` — valore `23`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:59`*

```basic
cnt = cnt + 1 : cn(cnt) = "DEEP_SKY_BLUE4" : ci(cnt) = %DEEP_SKY_BLUE4
```

### `%DEEP_SKY_BLUE4_2` — valore `24`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:60`*

```basic
cnt = cnt + 1 : cn(cnt) = "DEEP_SKY_BLUE4_2" : ci(cnt) = %DEEP_SKY_BLUE4_2
```

### `%DEEP_SKY_BLUE4_3` — valore `25`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:61`*

```basic
cnt = cnt + 1 : cn(cnt) = "DEEP_SKY_BLUE4_3" : ci(cnt) = %DEEP_SKY_BLUE4_3
```

### `%DODGER_BLUE1` — valore `33`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:69`*

```basic
cnt = cnt + 1 : cn(cnt) = "DODGER_BLUE1" : ci(cnt) = %DODGER_BLUE1
```

### `%DODGER_BLUE2` — valore `27`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:63`*

```basic
cnt = cnt + 1 : cn(cnt) = "DODGER_BLUE2" : ci(cnt) = %DODGER_BLUE2
```

### `%DODGER_BLUE3` — valore `26`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:62`*

```basic
cnt = cnt + 1 : cn(cnt) = "DODGER_BLUE3" : ci(cnt) = %DODGER_BLUE3
```

### `%DOUBLE` — valore `2`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:43`*

```basic
PRINT "  %DOUBLE"; TAB(24); %DOUBLE
```

### `%EMBEDDING` — valore `3`

*uso reale in `shared/samples/VectorDB/RagEvalE2eGentest.bas:17`*

```basic
AI REGISTER "llama1b-emb", %EMBEDDING, "mlx", "{""modelDir"":""~/ai/Models/Llama-3.2-1B-Instruct-4bit"",""maxTokens"":""8""}"
```

### `%FAST` — valore `1`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:90`*

```basic
PRINT "  %FAST"; TAB(24); %FAST
```

### `%FILE_CHANGED` — valore `4`

*uso reale in `shared/samples/System/SystemConstants.bas:17`*

```basic
PRINT "%FILE_CHANGED  = "; %FILE_CHANGED
```

### `%FILE_CREATED` — valore `1`

*uso reale in `shared/samples/System/SystemConstants.bas:6`*

```basic
' - %FILE_CREATED/DELETED/CHANGED/RENAMED → maschere eventi del WATCH (IO)
```

### `%FILE_DELETED` — valore `2`

*uso reale in `shared/samples/System/SystemConstants.bas:16`*

```basic
PRINT "%FILE_DELETED  = "; %FILE_DELETED
```

### `%FILE_RENAMED` — valore `8`

*uso reale in `shared/samples/System/SystemConstants.bas:18`*

```basic
PRINT "%FILE_RENAMED  = "; %FILE_RENAMED
```

### `%FUCHSIA` — valore `13`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:49`*

```basic
cnt = cnt + 1 : cn(cnt) = "FUCHSIA" : ci(cnt) = %FUCHSIA
```

### `%GOVERNED` — valore `2`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:91`*

```basic
PRINT "  %GOVERNED"; TAB(24); %GOVERNED
```

### `%GRAY` — valore `8`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:44`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY" : ci(cnt) = %GRAY
```

### `%GRAY0` — valore `16`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:52`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY0" : ci(cnt) = %GRAY0
```

### `%GRAY11` — valore `235`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:163`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY11" : ci(cnt) = %GRAY11
```

### `%GRAY15` — valore `236`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:164`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY15" : ci(cnt) = %GRAY15
```

### `%GRAY19` — valore `237`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:165`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY19" : ci(cnt) = %GRAY19
```

### `%GRAY2` — valore `232`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:160`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY2" : ci(cnt) = %GRAY2
```

### `%GRAY23` — valore `238`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:166`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY23" : ci(cnt) = %GRAY23
```

### `%GRAY27` — valore `239`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:167`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY27" : ci(cnt) = %GRAY27
```

### `%GRAY3` — valore `233`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:161`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY3" : ci(cnt) = %GRAY3
```

### `%GRAY30` — valore `240`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:168`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY30" : ci(cnt) = %GRAY30
```

### `%GRAY35` — valore `241`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:169`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY35" : ci(cnt) = %GRAY35
```

### `%GRAY37` — valore `59`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:95`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY37" : ci(cnt) = %GRAY37
```

### `%GRAY39` — valore `242`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:170`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY39" : ci(cnt) = %GRAY39
```

### `%GRAY42` — valore `243`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:171`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY42" : ci(cnt) = %GRAY42
```

### `%GRAY46` — valore `244`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:172`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY46" : ci(cnt) = %GRAY46
```

### `%GRAY50` — valore `245`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:173`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY50" : ci(cnt) = %GRAY50
```

### `%GRAY53` — valore `102`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:138`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY53" : ci(cnt) = %GRAY53
```

### `%GRAY54` — valore `246`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:174`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY54" : ci(cnt) = %GRAY54
```

### `%GRAY58` — valore `247`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:175`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY58" : ci(cnt) = %GRAY58
```

### `%GRAY62` — valore `248`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:176`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY62" : ci(cnt) = %GRAY62
```

### `%GRAY66` — valore `249`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:177`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY66" : ci(cnt) = %GRAY66
```

### `%GRAY7` — valore `234`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:162`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY7" : ci(cnt) = %GRAY7
```

### `%GRAY70` — valore `250`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:178`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY70" : ci(cnt) = %GRAY70
```

### `%GRAY74` — valore `251`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:179`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY74" : ci(cnt) = %GRAY74
```

### `%GRAY78` — valore `252`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:180`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY78" : ci(cnt) = %GRAY78
```

### `%GRAY82` — valore `253`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:181`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY82" : ci(cnt) = %GRAY82
```

### `%GRAY85` — valore `254`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:182`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY85" : ci(cnt) = %GRAY85
```

### `%GRAY89` — valore `255`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:183`*

```basic
cnt = cnt + 1 : cn(cnt) = "GRAY89" : ci(cnt) = %GRAY89
```

### `%GREEN` — valore `2`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:38`*

```basic
cnt = cnt + 1 : cn(cnt) = "GREEN" : ci(cnt) = %GREEN
```

### `%GREEN1` — valore `46`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:82`*

```basic
cnt = cnt + 1 : cn(cnt) = "GREEN1" : ci(cnt) = %GREEN1
```

### `%GREEN3` — valore `34`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:70`*

```basic
cnt = cnt + 1 : cn(cnt) = "GREEN3" : ci(cnt) = %GREEN3
```

### `%GREEN3_2` — valore `40`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:76`*

```basic
cnt = cnt + 1 : cn(cnt) = "GREEN3_2" : ci(cnt) = %GREEN3_2
```

### `%GREEN4` — valore `28`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:64`*

```basic
cnt = cnt + 1 : cn(cnt) = "GREEN4" : ci(cnt) = %GREEN4
```

### `%IGNORE` — valore `5`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:32`*

```basic
PRINT "  %IGNORE"; TAB(24); %IGNORE
```

### `%JOYBUTTON2` — valore `32`

*uso reale in `shared/samples/System/SystemConstants.bas:28`*

```basic
PRINT "%JOYBUTTON2 = "; %JOYBUTTON2
```

### `%JOYDOWN` — valore `2`

*uso reale in `shared/samples/System/SystemConstants.bas:24`*

```basic
PRINT "%JOYDOWN    = "; %JOYDOWN
```

### `%JOYFIRE` — valore `16`

*uso reale in `shared/samples/System/SystemConstants.bas:27`*

```basic
PRINT "%JOYFIRE    = "; %JOYFIRE
```

### `%JOYLEFT` — valore `4`

*uso reale in `shared/samples/System/SystemConstants.bas:25`*

```basic
PRINT "%JOYLEFT    = "; %JOYLEFT
```

### `%JOYRIGHT` — valore `8`

*uso reale in `shared/samples/System/SystemConstants.bas:26`*

```basic
PRINT "%JOYRIGHT   = "; %JOYRIGHT
```

### `%JOYUP` — valore `1`

*uso reale in `shared/samples/System/SystemConstants.bas:7`*

```basic
' - %JOYUP/DOWN/LEFT/RIGHT/FIRE/BUTTON2 → bit del joystick (JOY)
```

### `%K_0` — valore `48`

*uso reale in `shared/samples/System/SystemConstants.bas:8`*

```basic
' - %K_F1-%K_F24/%K_0-%K_9/%K_A-%K_Z/%K_ESC/%K_ENT/%K_ALT/%K_SHIFT/%K_CTRL/%K_CTRL_C → codici tasti ON KEY
```

### `%K_1` — valore `49`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:92`*

```basic
knum = knum + 1 : kn(knum) = "K_1" : kc(knum) = %K_1
```

### `%K_2` — valore `50`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:93`*

```basic
knum = knum + 1 : kn(knum) = "K_2" : kc(knum) = %K_2
```

### `%K_3` — valore `51`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:94`*

```basic
knum = knum + 1 : kn(knum) = "K_3" : kc(knum) = %K_3
```

### `%K_4` — valore `52`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:95`*

```basic
knum = knum + 1 : kn(knum) = "K_4" : kc(knum) = %K_4
```

### `%K_5` — valore `53`

*uso reale in `shared/samples/Events/OnKeyUncoveredCoverage.bas:40`*

```basic
PRINT "%K_5 = "; %K_5; " (53)"
```

### `%K_6` — valore `54`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:97`*

```basic
knum = knum + 1 : kn(knum) = "K_6" : kc(knum) = %K_6
```

### `%K_7` — valore `55`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:98`*

```basic
knum = knum + 1 : kn(knum) = "K_7" : kc(knum) = %K_7
```

### `%K_8` — valore `56`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:99`*

```basic
knum = knum + 1 : kn(knum) = "K_8" : kc(knum) = %K_8
```

### `%K_9` — valore `57`

*uso reale in `shared/samples/System/SystemConstants.bas:8`*

```basic
' - %K_F1-%K_F24/%K_0-%K_9/%K_A-%K_Z/%K_ESC/%K_ENT/%K_ALT/%K_SHIFT/%K_CTRL/%K_CTRL_C → codici tasti ON KEY
```

### `%K_A` — valore `65`

*uso reale in `shared/samples/System/SystemConstants.bas:8`*

```basic
' - %K_F1-%K_F24/%K_0-%K_9/%K_A-%K_Z/%K_ESC/%K_ENT/%K_ALT/%K_SHIFT/%K_CTRL/%K_CTRL_C → codici tasti ON KEY
```

### `%K_ALT` — valore `1`

*uso reale in `shared/samples/System/SystemConstants.bas:8`*

```basic
' - %K_F1-%K_F24/%K_0-%K_9/%K_A-%K_Z/%K_ESC/%K_ENT/%K_ALT/%K_SHIFT/%K_CTRL/%K_CTRL_C → codici tasti ON KEY
```

### `%K_B` — valore `66`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:66`*

```basic
knum = knum + 1 : kn(knum) = "K_B" : kc(knum) = %K_B
```

### `%K_BS` — valore `8`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:61`*

```basic
knum = knum + 1 : kn(knum) = "K_BS" : kc(knum) = %K_BS
```

### `%K_C` — valore `67`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:67`*

```basic
knum = knum + 1 : kn(knum) = "K_C" : kc(knum) = %K_C
```

### `%K_CTRL` — valore `4`

*uso reale in `shared/samples/System/SystemConstants.bas:8`*

```basic
' - %K_F1-%K_F24/%K_0-%K_9/%K_A-%K_Z/%K_ESC/%K_ENT/%K_ALT/%K_SHIFT/%K_CTRL/%K_CTRL_C → codici tasti ON KEY
```

### `%K_CTRL_BRK` — valore `-1`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:117`*

```basic
knum = knum + 1 : kn(knum) = "K_CTRL_BRK" : kc(knum) = %K_CTRL_BRK
```

### `%K_CTRL_C` — valore `-1`

*uso reale in `shared/samples/System/SystemConstants.bas:8`*

```basic
' - %K_F1-%K_F24/%K_0-%K_9/%K_A-%K_Z/%K_ESC/%K_ENT/%K_ALT/%K_SHIFT/%K_CTRL/%K_CTRL_C → codici tasti ON KEY
```

### `%K_D` — valore `68`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:68`*

```basic
knum = knum + 1 : kn(knum) = "K_D" : kc(knum) = %K_D
```

### `%K_DEL` — valore `46`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:57`*

```basic
knum = knum + 1 : kn(knum) = "K_DEL" : kc(knum) = %K_DEL
```

### `%K_DOWN` — valore `40`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:49`*

```basic
knum = knum + 1 : kn(knum) = "K_DOWN" : kc(knum) = %K_DOWN
```

### `%K_E` — valore `69`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:69`*

```basic
knum = knum + 1 : kn(knum) = "K_E" : kc(knum) = %K_E
```

### `%K_END` — valore `35`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:53`*

```basic
knum = knum + 1 : kn(knum) = "K_END" : kc(knum) = %K_END
```

### `%K_ENTER` — valore `13`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:60`*

```basic
knum = knum + 1 : kn(knum) = "K_ENTER" : kc(knum) = %K_ENTER
```

### `%K_ESC` — valore `27`

*uso reale in `shared/samples/Clipper/ClipperFullDemo.bas:18`*

```basic
ON KEY(%K_ESC) GOSUB DoQuit
```

### `%K_F` — valore `70`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:70`*

```basic
knum = knum + 1 : kn(knum) = "K_F" : kc(knum) = %K_F
```

### `%K_F1` — valore `112`

*uso reale in `shared/samples/Clipper/ClipperFullDemo.bas:17`*

```basic
ON KEY(%K_F1) GOSUB ShowHelp
```

### `%K_F10` — valore `121`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:45`*

```basic
knum = knum + 1 : kn(knum) = "K_F10" : kc(knum) = %K_F10
```

### `%K_F11` — valore `122`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:46`*

```basic
knum = knum + 1 : kn(knum) = "K_F11" : kc(knum) = %K_F11
```

### `%K_F12` — valore `123`

*uso reale in `shared/samples/System/SystemConstants.bas:35`*

```basic
PRINT "%K_F12      = "; %K_F12
```

### `%K_F2` — valore `113`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:37`*

```basic
knum = knum + 1 : kn(knum) = "K_F2" : kc(knum) = %K_F2
```

### `%K_F3` — valore `114`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:38`*

```basic
knum = knum + 1 : kn(knum) = "K_F3" : kc(knum) = %K_F3
```

### `%K_F4` — valore `115`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:39`*

```basic
knum = knum + 1 : kn(knum) = "K_F4" : kc(knum) = %K_F4
```

### `%K_F5` — valore `116`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:40`*

```basic
knum = knum + 1 : kn(knum) = "K_F5" : kc(knum) = %K_F5
```

### `%K_F6` — valore `117`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:41`*

```basic
knum = knum + 1 : kn(knum) = "K_F6" : kc(knum) = %K_F6
```

### `%K_F7` — valore `118`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:42`*

```basic
knum = knum + 1 : kn(knum) = "K_F7" : kc(knum) = %K_F7
```

### `%K_F8` — valore `119`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:43`*

```basic
knum = knum + 1 : kn(knum) = "K_F8" : kc(knum) = %K_F8
```

### `%K_F9` — valore `120`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:44`*

```basic
knum = knum + 1 : kn(knum) = "K_F9" : kc(knum) = %K_F9
```

### `%K_G` — valore `71`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:71`*

```basic
knum = knum + 1 : kn(knum) = "K_G" : kc(knum) = %K_G
```

### `%K_H` — valore `72`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:72`*

```basic
knum = knum + 1 : kn(knum) = "K_H" : kc(knum) = %K_H
```

### `%K_HOME` — valore `36`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:52`*

```basic
knum = knum + 1 : kn(knum) = "K_HOME" : kc(knum) = %K_HOME
```

### `%K_I` — valore `73`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:73`*

```basic
knum = knum + 1 : kn(knum) = "K_I" : kc(knum) = %K_I
```

### `%K_INS` — valore `45`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:56`*

```basic
knum = knum + 1 : kn(knum) = "K_INS" : kc(knum) = %K_INS
```

### `%K_J` — valore `74`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:74`*

```basic
knum = knum + 1 : kn(knum) = "K_J" : kc(knum) = %K_J
```

### `%K_K` — valore `75`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:75`*

```basic
knum = knum + 1 : kn(knum) = "K_K" : kc(knum) = %K_K
```

### `%K_L` — valore `76`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:76`*

```basic
knum = knum + 1 : kn(knum) = "K_L" : kc(knum) = %K_L
```

### `%K_LEFT` — valore `37`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:50`*

```basic
knum = knum + 1 : kn(knum) = "K_LEFT" : kc(knum) = %K_LEFT
```

### `%K_M` — valore `77`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:77`*

```basic
knum = knum + 1 : kn(knum) = "K_M" : kc(knum) = %K_M
```

### `%K_N` — valore `78`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:78`*

```basic
knum = knum + 1 : kn(knum) = "K_N" : kc(knum) = %K_N
```

### `%K_N0` — valore `96`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:101`*

```basic
knum = knum + 1 : kn(knum) = "K_N0" : kc(knum) = %K_N0
```

### `%K_N1` — valore `97`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:102`*

```basic
knum = knum + 1 : kn(knum) = "K_N1" : kc(knum) = %K_N1
```

### `%K_N2` — valore `98`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:103`*

```basic
knum = knum + 1 : kn(knum) = "K_N2" : kc(knum) = %K_N2
```

### `%K_N3` — valore `99`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:104`*

```basic
knum = knum + 1 : kn(knum) = "K_N3" : kc(knum) = %K_N3
```

### `%K_N4` — valore `100`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:105`*

```basic
knum = knum + 1 : kn(knum) = "K_N4" : kc(knum) = %K_N4
```

### `%K_N5` — valore `101`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:106`*

```basic
knum = knum + 1 : kn(knum) = "K_N5" : kc(knum) = %K_N5
```

### `%K_N6` — valore `102`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:107`*

```basic
knum = knum + 1 : kn(knum) = "K_N6" : kc(knum) = %K_N6
```

### `%K_N7` — valore `103`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:108`*

```basic
knum = knum + 1 : kn(knum) = "K_N7" : kc(knum) = %K_N7
```

### `%K_N8` — valore `104`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:109`*

```basic
knum = knum + 1 : kn(knum) = "K_N8" : kc(knum) = %K_N8
```

### `%K_N9` — valore `105`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:110`*

```basic
knum = knum + 1 : kn(knum) = "K_N9" : kc(knum) = %K_N9
```

### `%K_NADD` — valore `107`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:114`*

```basic
knum = knum + 1 : kn(knum) = "K_NADD" : kc(knum) = %K_NADD
```

### `%K_NDEC` — valore `110`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:115`*

```basic
knum = knum + 1 : kn(knum) = "K_NDEC" : kc(knum) = %K_NDEC
```

### `%K_NDIV` — valore `111`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:111`*

```basic
knum = knum + 1 : kn(knum) = "K_NDIV" : kc(knum) = %K_NDIV
```

### `%K_NMUL` — valore `106`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:112`*

```basic
knum = knum + 1 : kn(knum) = "K_NMUL" : kc(knum) = %K_NMUL
```

### `%K_NSUB` — valore `109`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:113`*

```basic
knum = knum + 1 : kn(knum) = "K_NSUB" : kc(knum) = %K_NSUB
```

### `%K_O` — valore `79`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:79`*

```basic
knum = knum + 1 : kn(knum) = "K_O" : kc(knum) = %K_O
```

### `%K_P` — valore `80`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:80`*

```basic
knum = knum + 1 : kn(knum) = "K_P" : kc(knum) = %K_P
```

### `%K_PAUSE` — valore `19`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:63`*

```basic
knum = knum + 1 : kn(knum) = "K_PAUSE" : kc(knum) = %K_PAUSE
```

### `%K_PGDN` — valore `34`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:55`*

```basic
knum = knum + 1 : kn(knum) = "K_PGDN" : kc(knum) = %K_PGDN
```

### `%K_PGUP` — valore `33`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:54`*

```basic
knum = knum + 1 : kn(knum) = "K_PGUP" : kc(knum) = %K_PGUP
```

### `%K_PRTSC` — valore `44`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:64`*

```basic
knum = knum + 1 : kn(knum) = "K_PRTSC" : kc(knum) = %K_PRTSC
```

### `%K_Q` — valore `81`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:81`*

```basic
knum = knum + 1 : kn(knum) = "K_Q" : kc(knum) = %K_Q
```

### `%K_R` — valore `82`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:82`*

```basic
knum = knum + 1 : kn(knum) = "K_R" : kc(knum) = %K_R
```

### `%K_RIGHT` — valore `39`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:51`*

```basic
knum = knum + 1 : kn(knum) = "K_RIGHT" : kc(knum) = %K_RIGHT
```

### `%K_S` — valore `83`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:83`*

```basic
knum = knum + 1 : kn(knum) = "K_S" : kc(knum) = %K_S
```

### `%K_SHIFT` — valore `2`

*uso reale in `shared/samples/System/SystemConstants.bas:8`*

```basic
' - %K_F1-%K_F24/%K_0-%K_9/%K_A-%K_Z/%K_ESC/%K_ENT/%K_ALT/%K_SHIFT/%K_CTRL/%K_CTRL_C → codici tasti ON KEY
```

### `%K_SHIFT_TAB` — valore `-1`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:118`*

```basic
knum = knum + 1 : kn(knum) = "K_SHIFT_TAB" : kc(knum) = %K_SHIFT_TAB
```

### `%K_SPC` — valore `32`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:62`*

```basic
knum = knum + 1 : kn(knum) = "K_SPC" : kc(knum) = %K_SPC
```

### `%K_T` — valore `84`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:84`*

```basic
knum = knum + 1 : kn(knum) = "K_T" : kc(knum) = %K_T
```

### `%K_TAB` — valore `9`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:59`*

```basic
knum = knum + 1 : kn(knum) = "K_TAB" : kc(knum) = %K_TAB
```

### `%K_U` — valore `85`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:85`*

```basic
knum = knum + 1 : kn(knum) = "K_U" : kc(knum) = %K_U
```

### `%K_UP` — valore `38`

*uso reale in `shared/samples/Console/ConsoleOnKeyClipboard.bas:54`*

```basic
30 ON KEY (%K_UP) GOTO FrecciaSu
```

### `%K_V` — valore `86`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:86`*

```basic
knum = knum + 1 : kn(knum) = "K_V" : kc(knum) = %K_V
```

### `%K_W` — valore `87`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:87`*

```basic
knum = knum + 1 : kn(knum) = "K_W" : kc(knum) = %K_W
```

### `%K_X` — valore `88`

*uso reale in `shared/samples/Console/ConsoleOnKeyClipboard.bas:15`*

```basic
'      singoli (`%K_F1`) e combinazioni con modificatori tramite l'helper `KEY.MAKE(%K_X, %K_CTRL)`.
```

### `%K_Y` — valore `89`

*uso reale in `shared/samples/Console/ConstantsKeysCoverage.bas:89`*

```basic
knum = knum + 1 : kn(knum) = "K_Y" : kc(knum) = %K_Y
```

### `%K_Z` — valore `90`

*uso reale in `shared/samples/System/SystemConstants.bas:8`*

```basic
' - %K_F1-%K_F24/%K_0-%K_9/%K_A-%K_Z/%K_ESC/%K_ENT/%K_ALT/%K_SHIFT/%K_CTRL/%K_CTRL_C → codici tasti ON KEY
```

### `%LEFT` — valore `0`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:94`*

```basic
PRINT "  %LEFT"; TAB(24); %LEFT
```

### `%LIGHT_GREEN` — valore `119`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:155`*

```basic
cnt = cnt + 1 : cn(cnt) = "LIGHT_GREEN" : ci(cnt) = %LIGHT_GREEN
```

### `%LIGHT_GREEN_2` — valore `120`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:156`*

```basic
cnt = cnt + 1 : cn(cnt) = "LIGHT_GREEN_2" : ci(cnt) = %LIGHT_GREEN_2
```

### `%LIGHT_PINK4` — valore `95`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:131`*

```basic
cnt = cnt + 1 : cn(cnt) = "LIGHT_PINK4" : ci(cnt) = %LIGHT_PINK4
```

### `%LIGHT_SEA_GREEN` — valore `37`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:73`*

```basic
cnt = cnt + 1 : cn(cnt) = "LIGHT_SEA_GREEN" : ci(cnt) = %LIGHT_SEA_GREEN
```

### `%LIGHT_SKY_BLUE3` — valore `109`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:145`*

```basic
cnt = cnt + 1 : cn(cnt) = "LIGHT_SKY_BLUE3" : ci(cnt) = %LIGHT_SKY_BLUE3
```

### `%LIGHT_SKY_BLUE3_2` — valore `110`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:146`*

```basic
cnt = cnt + 1 : cn(cnt) = "LIGHT_SKY_BLUE3_2" : ci(cnt) = %LIGHT_SKY_BLUE3_2
```

### `%LIGHT_SLATE_BLUE` — valore `105`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:141`*

```basic
cnt = cnt + 1 : cn(cnt) = "LIGHT_SLATE_BLUE" : ci(cnt) = %LIGHT_SLATE_BLUE
```

### `%LIGHT_SLATE_GRAY` — valore `103`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:139`*

```basic
cnt = cnt + 1 : cn(cnt) = "LIGHT_SLATE_GRAY" : ci(cnt) = %LIGHT_SLATE_GRAY
```

### `%LIME` — valore `10`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:46`*

```basic
cnt = cnt + 1 : cn(cnt) = "LIME" : ci(cnt) = %LIME
```

### `%MAROON` — valore `1`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:37`*

```basic
cnt = cnt + 1 : cn(cnt) = "MAROON" : ci(cnt) = %MAROON
```

### `%MEDIUM_PURPLE` — valore `104`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:140`*

```basic
cnt = cnt + 1 : cn(cnt) = "MEDIUM_PURPLE" : ci(cnt) = %MEDIUM_PURPLE
```

### `%MEDIUM_PURPLE3` — valore `97`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:133`*

```basic
cnt = cnt + 1 : cn(cnt) = "MEDIUM_PURPLE3" : ci(cnt) = %MEDIUM_PURPLE3
```

### `%MEDIUM_PURPLE3_2` — valore `98`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:134`*

```basic
cnt = cnt + 1 : cn(cnt) = "MEDIUM_PURPLE3_2" : ci(cnt) = %MEDIUM_PURPLE3_2
```

### `%MEDIUM_PURPLE4` — valore `60`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:96`*

```basic
cnt = cnt + 1 : cn(cnt) = "MEDIUM_PURPLE4" : ci(cnt) = %MEDIUM_PURPLE4
```

### `%MEDIUM_SPRING_GREEN` — valore `49`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:85`*

```basic
cnt = cnt + 1 : cn(cnt) = "MEDIUM_SPRING_GREEN" : ci(cnt) = %MEDIUM_SPRING_GREEN
```

### `%MEDIUM_TURQUOISE` — valore `80`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:116`*

```basic
cnt = cnt + 1 : cn(cnt) = "MEDIUM_TURQUOISE" : ci(cnt) = %MEDIUM_TURQUOISE
```

### `%NAVY` — valore `4`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:40`*

```basic
cnt = cnt + 1 : cn(cnt) = "NAVY" : ci(cnt) = %NAVY
```

### `%NAVY_BLUE` — valore `17`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:53`*

```basic
cnt = cnt + 1 : cn(cnt) = "NAVY_BLUE" : ci(cnt) = %NAVY_BLUE
```

### `%NO` — valore `7`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:34`*

```basic
PRINT "  %NO"; TAB(24); %NO
```

### `%NONE` — valore `0`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:41`*

```basic
PRINT "  %NONE"; TAB(24); %NONE
```

### `%OBSCURA` — valore `2`

*uso reale in `shared/samples/Ai/BrowserProviderPerAgent.bas:14`*

```basic
'    Sintassi: AI AGENT SET "nome" BROWSER %OBSCURA|%CHROMIUM|%AUTO (senza virgole)
```

### `%OCR` — valore `1`

*uso reale in `shared/samples/Ai/AiEmbeddedRegisterRouting.bas:20`*

```basic
'    - %CHAT/%OCR/%AUDIO (0/1/2) sono costanti predefinite usabili ovunque l'argomento
```

### `%OK` — valore `1`

*uso reale in `shared/samples/Clipper/ClipperAdvanced.bas:78`*

```basic
btnClicked = CONFIRM("Click Me") = %OK
```

### `%OKCANCEL` — valore `101`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:20`*

```basic
' USO: ALERT("msg", %OKCANCEL) — AI.SEARCHMULTI(..., %RERANK_KEYWORD)
```

### `%OLIVE` — valore `3`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:39`*

```basic
cnt = cnt + 1 : cn(cnt) = "OLIVE" : ci(cnt) = %OLIVE
```

### `%ORANGE4` — valore `58`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:94`*

```basic
cnt = cnt + 1 : cn(cnt) = "ORANGE4" : ci(cnt) = %ORANGE4
```

### `%ORANGE4_2` — valore `94`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:130`*

```basic
cnt = cnt + 1 : cn(cnt) = "ORANGE4_2" : ci(cnt) = %ORANGE4_2
```

### `%PALE_GREEN1` — valore `121`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:157`*

```basic
cnt = cnt + 1 : cn(cnt) = "PALE_GREEN1" : ci(cnt) = %PALE_GREEN1
```

### `%PALE_GREEN3` — valore `77`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:113`*

```basic
cnt = cnt + 1 : cn(cnt) = "PALE_GREEN3" : ci(cnt) = %PALE_GREEN3
```

### `%PALE_GREEN3_2` — valore `114`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:150`*

```basic
cnt = cnt + 1 : cn(cnt) = "PALE_GREEN3_2" : ci(cnt) = %PALE_GREEN3_2
```

### `%PALE_TURQUOISE4` — valore `66`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:102`*

```basic
cnt = cnt + 1 : cn(cnt) = "PALE_TURQUOISE4" : ci(cnt) = %PALE_TURQUOISE4
```

### `%PLUM4` — valore `96`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:132`*

```basic
cnt = cnt + 1 : cn(cnt) = "PLUM4" : ci(cnt) = %PLUM4
```

### `%PURPLE` — valore `5`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:41`*

```basic
cnt = cnt + 1 : cn(cnt) = "PURPLE" : ci(cnt) = %PURPLE
```

### `%PURPLE1` — valore `93`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:129`*

```basic
cnt = cnt + 1 : cn(cnt) = "PURPLE1" : ci(cnt) = %PURPLE1
```

### `%PURPLE3` — valore `56`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:92`*

```basic
cnt = cnt + 1 : cn(cnt) = "PURPLE3" : ci(cnt) = %PURPLE3
```

### `%PURPLE4` — valore `54`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:90`*

```basic
cnt = cnt + 1 : cn(cnt) = "PURPLE4" : ci(cnt) = %PURPLE4
```

### `%PURPLE4_2` — valore `55`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:91`*

```basic
cnt = cnt + 1 : cn(cnt) = "PURPLE4_2" : ci(cnt) = %PURPLE4_2
```

### `%RECEIVED` — valore `1`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:85`*

```basic
PRINT "  %RECEIVED"; TAB(24); %RECEIVED
```

### `%RED` — valore `9`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:45`*

```basic
cnt = cnt + 1 : cn(cnt) = "RED" : ci(cnt) = %RED
```

### `%RERANKER` — valore `4`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:71`*

```basic
PRINT "  %RERANKER"; TAB(24); %RERANKER
```

### `%RERANK_KEYWORD` — valore `1`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:20`*

```basic
' USO: ALERT("msg", %OKCANCEL) — AI.SEARCHMULTI(..., %RERANK_KEYWORD)
```

### `%RERANK_OFF` — valore `0`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:75`*

```basic
PRINT "  %RERANK_OFF"; TAB(24); %RERANK_OFF
```

### `%RERANK_ONNX` — valore `2`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:77`*

```basic
PRINT "  %RERANK_ONNX"; TAB(24); %RERANK_ONNX
```

### `%RETRY` — valore `4`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:31`*

```basic
PRINT "  %RETRY"; TAB(24); %RETRY
```

### `%RETRYCANCEL` — valore `105`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:39`*

```basic
PRINT "  %RETRYCANCEL"; TAB(24); %RETRYCANCEL
```

### `%RIGHT` — valore `2`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:96`*

```basic
PRINT "  %RIGHT"; TAB(24); %RIGHT
```

### `%ROUNDED` — valore `3`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:44`*

```basic
PRINT "  %ROUNDED"; TAB(24); %ROUNDED
```

### `%ROYAL_BLUE1` — valore `63`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:99`*

```basic
cnt = cnt + 1 : cn(cnt) = "ROYAL_BLUE1" : ci(cnt) = %ROYAL_BLUE1
```

### `%SDL` — valore `0`

*uso reale in `shared/samples/graphics/ConsoleModeSelect.bas:3`*

```basic
' TEXTMODE — selezione della modalita' di console a runtime + costanti %SDL/%C64/%ANSI/%SYSTEM
```

### `%SEA_GREEN1` — valore `84`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:120`*

```basic
cnt = cnt + 1 : cn(cnt) = "SEA_GREEN1" : ci(cnt) = %SEA_GREEN1
```

### `%SEA_GREEN1_2` — valore `85`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:121`*

```basic
cnt = cnt + 1 : cn(cnt) = "SEA_GREEN1_2" : ci(cnt) = %SEA_GREEN1_2
```

### `%SEA_GREEN2` — valore `83`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:119`*

```basic
cnt = cnt + 1 : cn(cnt) = "SEA_GREEN2" : ci(cnt) = %SEA_GREEN2
```

### `%SEA_GREEN3` — valore `78`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:114`*

```basic
cnt = cnt + 1 : cn(cnt) = "SEA_GREEN3" : ci(cnt) = %SEA_GREEN3
```

### `%SILVER` — valore `7`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:43`*

```basic
cnt = cnt + 1 : cn(cnt) = "SILVER" : ci(cnt) = %SILVER
```

### `%SINGLE` — valore `1`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:42`*

```basic
PRINT "  %SINGLE"; TAB(24); %SINGLE
```

### `%SKY_BLUE1` — valore `117`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:153`*

```basic
cnt = cnt + 1 : cn(cnt) = "SKY_BLUE1" : ci(cnt) = %SKY_BLUE1
```

### `%SKY_BLUE2` — valore `111`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:147`*

```basic
cnt = cnt + 1 : cn(cnt) = "SKY_BLUE2" : ci(cnt) = %SKY_BLUE2
```

### `%SKY_BLUE3` — valore `74`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:110`*

```basic
cnt = cnt + 1 : cn(cnt) = "SKY_BLUE3" : ci(cnt) = %SKY_BLUE3
```

### `%SLATE_BLUE1` — valore `99`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:135`*

```basic
cnt = cnt + 1 : cn(cnt) = "SLATE_BLUE1" : ci(cnt) = %SLATE_BLUE1
```

### `%SLATE_BLUE3` — valore `61`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:97`*

```basic
cnt = cnt + 1 : cn(cnt) = "SLATE_BLUE3" : ci(cnt) = %SLATE_BLUE3
```

### `%SLATE_BLUE3_2` — valore `62`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:98`*

```basic
cnt = cnt + 1 : cn(cnt) = "SLATE_BLUE3_2" : ci(cnt) = %SLATE_BLUE3_2
```

### `%SPRING_GREEN1` — valore `48`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:84`*

```basic
cnt = cnt + 1 : cn(cnt) = "SPRING_GREEN1" : ci(cnt) = %SPRING_GREEN1
```

### `%SPRING_GREEN2` — valore `42`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:78`*

```basic
cnt = cnt + 1 : cn(cnt) = "SPRING_GREEN2" : ci(cnt) = %SPRING_GREEN2
```

### `%SPRING_GREEN2_2` — valore `47`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:83`*

```basic
cnt = cnt + 1 : cn(cnt) = "SPRING_GREEN2_2" : ci(cnt) = %SPRING_GREEN2_2
```

### `%SPRING_GREEN3` — valore `35`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:71`*

```basic
cnt = cnt + 1 : cn(cnt) = "SPRING_GREEN3" : ci(cnt) = %SPRING_GREEN3
```

### `%SPRING_GREEN3_2` — valore `41`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:77`*

```basic
cnt = cnt + 1 : cn(cnt) = "SPRING_GREEN3_2" : ci(cnt) = %SPRING_GREEN3_2
```

### `%SPRING_GREEN4` — valore `29`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:65`*

```basic
cnt = cnt + 1 : cn(cnt) = "SPRING_GREEN4" : ci(cnt) = %SPRING_GREEN4
```

### `%STEEL_BLUE` — valore `67`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:103`*

```basic
cnt = cnt + 1 : cn(cnt) = "STEEL_BLUE" : ci(cnt) = %STEEL_BLUE
```

### `%STEEL_BLUE1` — valore `75`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:111`*

```basic
cnt = cnt + 1 : cn(cnt) = "STEEL_BLUE1" : ci(cnt) = %STEEL_BLUE1
```

### `%STEEL_BLUE1_2` — valore `81`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:117`*

```basic
cnt = cnt + 1 : cn(cnt) = "STEEL_BLUE1_2" : ci(cnt) = %STEEL_BLUE1_2
```

### `%STEEL_BLUE3` — valore `68`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:104`*

```basic
cnt = cnt + 1 : cn(cnt) = "STEEL_BLUE3" : ci(cnt) = %STEEL_BLUE3
```

### `%SYSTEM` — valore `3`

*uso reale in `shared/samples/graphics/ConsoleModeSelect.bas:3`*

```basic
' TEXTMODE — selezione della modalita' di console a runtime + costanti %SDL/%C64/%ANSI/%SYSTEM
```

### `%TEAL` — valore `6`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:42`*

```basic
cnt = cnt + 1 : cn(cnt) = "TEAL" : ci(cnt) = %TEAL
```

### `%TTS` — valore `5`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:72`*

```basic
PRINT "  %TTS"; TAB(24); %TTS
```

### `%TURQUOISE2` — valore `45`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:81`*

```basic
cnt = cnt + 1 : cn(cnt) = "TURQUOISE2" : ci(cnt) = %TURQUOISE2
```

### `%TURQUOISE4` — valore `30`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:66`*

```basic
cnt = cnt + 1 : cn(cnt) = "TURQUOISE4" : ci(cnt) = %TURQUOISE4
```

### `%WHEAT4` — valore `101`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:137`*

```basic
cnt = cnt + 1 : cn(cnt) = "WHEAT4" : ci(cnt) = %WHEAT4
```

### `%WHITE` — valore `15`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:51`*

```basic
cnt = cnt + 1 : cn(cnt) = "WHITE" : ci(cnt) = %WHITE
```

### `%YELLOW` — valore `11`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:47`*

```basic
cnt = cnt + 1 : cn(cnt) = "YELLOW" : ci(cnt) = %YELLOW
```

### `%YELLOW4` — valore `100`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:136`*

```basic
cnt = cnt + 1 : cn(cnt) = "YELLOW4" : ci(cnt) = %YELLOW4
```

### `%YELLOW4_2` — valore `106`

*uso reale in `shared/samples/Console/ConstantsColorsCoverage.bas:142`*

```basic
cnt = cnt + 1 : cn(cnt) = "YELLOW4_2" : ci(cnt) = %YELLOW4_2
```

### `%YES` — valore `6`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:33`*

```basic
PRINT "  %YES"; TAB(24); %YES
```

### `%YESNO` — valore `102`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:36`*

```basic
PRINT "  %YESNO"; TAB(24); %YESNO
```

### `%YESNOCANCEL` — valore `103`

*uso reale in `shared/samples/Console/ConstantsModesCoverage.bas:37`*

```basic
PRINT "  %YESNOCANCEL"; TAB(24); %YESNOCANCEL
```


## Tipi e record

### `__TempMacro`

*definizione da `shared/samples/Clipper/ClipperMacro.bas`*

```basic
    TYPE __TempMacro
        Nome AS STRING * 15
        Citta AS STRING * 15
        Valore1 AS INTEGER
        Valore2 AS INTEGER
        Attivo AS BIT
    END TYPE
```

### `__TempStructure`

*definizione da `shared/samples/Clipper/ClipperDbfLateBinding.bas`*

```basic
    TYPE __TempStructure
        ID AS INTEGER
        Nome AS STRING * 20
        Citta AS STRING * 15
        Saldo AS CURRENCY
        Attivo AS BIT
        UltimoOrdine AS DATE
    END TYPE
```

### `__TempUpdate`

*definizione da `shared/samples/Clipper/ClipperMacro.bas`*

```basic
    TYPE __TempUpdate
        ID_Ext AS INTEGER
        NuovoValore AS INTEGER
    END TYPE
```

### `Arr4`

*definizione da `shared/minitests/ffi/ffi_udt_fieldarray.bas`*

```basic
TYPE Arr4
    A(4) AS LONG
    B AS DOUBLE
END TYPE
```

```basic
14:    DIM v AS Arr4
```

### `ArrMix`

*definizione da `shared/minitests/ffi/ffi_udt_fieldarray2.bas`*

```basic
TYPE ArrMix
    A(4) AS LONG
    D(2) AS DOUBLE
    B AS DOUBLE
END TYPE
```

```basic
15:    DIM v AS ArrMix
```

### `Big12`

*definizione da `shared/minitests/ffi/ffi_udt_many.bas`*

```basic
TYPE Big12
    F0 AS LONG
    F1 AS LONG
    F2 AS LONG
    F3 AS LONG
    F4 AS LONG
    F5 AS LONG
    F6 AS LONG
    F7 AS LONG
    F8 AS LONG
    F9 AS LONG
    F10 AS LONG
    F11 AS LONG
END TYPE
```

```basic
26:    DIM b AS Big12
```

### `Blob`

*definizione da `shared/minitests/ffi/ffi_udt_combo.bas`*

```basic
TYPE Blob
    P AS Pt2
    K AS LONG
    Z AS DOUBLE
    A(2) AS DOUBLE
    N AS QUAD
END TYPE
```

```basic
21:    DIM b AS Blob
```

### `Box`

*definizione da `shared/minitests/types/udt_varlen.bas`*

```basic
TYPE Box
  name AS STRING * 16
  tags AS LIST OF DOUBLE
  meta AS DICT OF STRING, DOUBLE
  note AS STRING
END TYPE
```

```basic
10:  DIM b AS Box
```

### `BufferT`

*definizione da `shared/samples/Classes/ClassesDestructor.bas`*

```basic
TYPE BufferT
    Id AS LONG
    Size AS LONG
    SUB NEW(id AS LONG, size AS LONG)
        SELF.Id = id
        SELF.Size = size
    END SUB
    SUB DESTRUCTOR
        PRINT "  [buffer liberato] id="; SELF.Id; " size="; SELF.Size
    END SUB
END TYPE
```

```basic
101:DIM gCache AS BufferT = NEW BufferT(99, 8192)
```

### `ByrefPt`

*definizione da `shared/minitests/ffi/ffi_byref_udt.bas`*

```basic
TYPE ByrefPt
    X AS DOUBLE
    Y AS DOUBLE
END TYPE
```

```basic
13:    DIM p AS ByrefPt
```

### `Cell`

*definizione da `shared/minitests/arrays/lbound_ubound_dims.bas`*

```basic
TYPE Cell
  V AS DOUBLE
  Tag AS STRING
END TYPE
```

### `Cella`

*definizione da `shared/minitests/types/udt_external_nested_access.bas`*

```basic
TYPE Cella
  valore AS INTEGER
  tag AS STRING * 4
END TYPE
```

### `Cerchio`

*definizione da `shared/samples/Types/TypesImplementsInterface.bas`*

```basic
TYPE Cerchio IMPLEMENTS IShape
    Raggio AS DOUBLE
    FUNCTION Area() AS DOUBLE
        AREA = 3.14159 * SELF.Raggio * SELF.Raggio
    END FUNCTION
    FUNCTION Describe() AS STRING
        DESCRIBE = "Circle(r=" + STR(SELF.Raggio) + ")"
    END FUNCTION
END TYPE
```

```basic
47:DIM c AS Cerchio
```

### `Cfg`

*definizione da `shared/minitests/json/commands_variants.bas`*

```basic
TYPE Cfg
  Host AS STRING * 50
  Timeout AS LONG
END TYPE
```

```basic
21:  DIM c1 AS Cfg : c1.Host = "h1" : c1.Timeout = 1
```

### `CfgData`

*definizione da `shared/minitests/json/json_all.bas`*

```basic
TYPE CfgData
  Host AS STRING * 50
  Timeout AS LONG
END TYPE
```

```basic
71:  DIM u AS CfgData
```

### `Cliente`

*definizione da `shared/minitests/speed/select.bas`*

```basic
TYPE Cliente
  ID AS LONG
  Nome AS STRING * 50
  Citta AS STRING * 30
  Saldo AS CURRENCY
  Attivo AS BIT
END TYPE
```

```basic
38:  DIM c AS Cliente
```

### `CmdConfig`

*definizione da `shared/minitests/json/commands.bas`*

```basic
TYPE CmdConfig
  Host AS STRING * 50
  Timeout AS LONG
END TYPE
```

```basic
16:  DIM cfg AS CmdConfig
```

### `ConfigData`

*definizione da `shared/samples/Json/JsonFullTests.bas`*

```basic
TYPE ConfigData
    Host AS STRING * 50
    Timeout AS LONG
    Ports(1) AS INTEGER
END TYPE
```

```basic
100:    DIM Cfg AS ConfigData
```

### `Contatto`

*definizione da `shared/minitests/constructs/expr_nullconditional.bas`*

```basic
TYPE Contatto
    Nome AS STRING * 32
    Eta  AS INTEGER
END TYPE
```

```basic
7:    DIM c AS Contatto?
```

### `Coord`

*definizione da `shared/minitests/types/udt_nested_2level.bas`*

```basic
TYPE Coord
  lat AS DOUBLE
  lon AS DOUBLE
END TYPE
```

### `Cplx16`

*definizione da `shared/minitests/ffi/ffi_udt_double_after_udts_long.bas`*

```basic
TYPE Cplx16
    Re AS DOUBLE
    Im AS DOUBLE
END TYPE
```

```basic
10:    DIM a AS Cplx16
```

### `Ctx`

*definizione da `shared/samples/Ai/BrainMultiBrain.bas`*

```basic
TYPE Ctx
    Usati AS STRING * 128
    Primo AS STRING * 64
    NoteOk AS INTEGER
END TYPE
```

```basic
32:    DIM c AS Ctx
```

### `Cust`

*definizione da `shared/samples/Databases/DbfRelationJoin.bas`*

```basic
TYPE Cust
    ID AS STRING * 10
    NAME AS STRING * 10
END TYPE
```

### `CustT`

*definizione da `shared/samples/Databases/RDD_Relation.bas`*

```basic
TYPE CustT
    ID AS STRING * 8
    Name AS STRING * 32
END TYPE
```

```basic
19:DIM c AS CustT
```

### `DatiCliente`

*definizione da `shared/samples/Files/FilesIO.bas`*

```basic
TYPE DatiCliente
  ID AS LONG
  Nome AS STRING * 20
  Saldo AS CURRENCY
  Attivo AS BIT
END TYPE
```

```basic
54:20 DIM CLIENTE AS DatiCliente, CLIENTE_LETTO AS DatiCliente
```

### `DbfTestType`

*definizione da `shared/samples/Databases/DbIsamFullTests.bas`*

```basic
    TYPE DbfTestType
        Codice AS LONG
        Descrizione AS STRING * 30
        Valore AS CURRENCY
    END TYPE
```

```basic
612:    DIM dbfTestRec AS DbfTestType
```

### `DelT`

*definizione da `shared/samples/Databases/RDD_SetDeleted.bas`*

```basic
TYPE DelT
    ID AS STRING * 8
    Val AS LONG
END TYPE
```

```basic
22:DIM d AS DelT
```

### `DRec`

*definizione da `shared/minitests/dbf/isam_double_roundtrip.bas`*

```basic
TYPE DRec
    Code AS STRING * 8
    Price AS DOUBLE
    Qty AS LONG
END TYPE
```

```basic
13:DIM p AS DRec
```

### `Employee`

*definizione da `shared/samples/Databases/RDD_Buffer_Demo.bas`*

```basic
TYPE Employee
    ID AS STRING * 8
    Name AS STRING * 32
    Salary AS DOUBLE
    Dept AS STRING * 16
END TYPE
```

```basic
23:DIM e AS Employee
```

### `Empty`

*definizione da `shared/minitests/system/system_sizeof_forms.bas`*

```basic
TYPE Empty
END TYPE
```

```basic
57:  DIM e AS Empty : PRINT SIZEOF(e)
```

### `FastRec`

*definizione da `shared/samples/Databases/RDD_Complete.bas`*

```basic
TYPE FastRec
    Code AS STRING * 8
    Score AS LONG
END TYPE
```

```basic
94:DIM fr AS FastRec
```

### `Fiz`

*definizione da `shared/minitests/ffi/ffi_udt_layout.bas`*

```basic
TYPE Fiz
    X AS SINGLE
    Y AS INTEGER
    Z AS QUAD
END TYPE
```

```basic
58:    DIM f AS Fiz
```

### `Grande`

*definizione da `shared/minitests/io/random_udt_record_toolong.bas`*

```basic
TYPE Grande
  A AS LONG
  Nome AS STRING * 40
END TYPE
```

```basic
13:  DIM g AS Grande, h AS Grande
```

### `HasCurrency`

*definizione da `shared/minitests/system/system_sizeof_forms.bas`*

```basic
TYPE HasCurrency
  a AS INTEGER
  c AS CURRENCY
END TYPE
```

```basic
77:  DIM cur AS HasCurrency : PRINT SYSTEM.UDTPACKED(cur)
```

### `Holding`

*definizione da `shared/samples/Classes/ClassesArrayMembers.bas`*

```basic
TYPE Holding
  shares AS INTEGER
  cost AS DOUBLE
END TYPE
```

### `I16`

*definizione da `shared/minitests/ffi/ffi_udt_double_after_udts.bas`*

```basic
TYPE I16
    A AS QUAD
    B AS QUAD
END TYPE
```

```basic
15:    DIM p AS I16
```

### `Inner`

*definizione da `shared/minitests/types/nested_udt_field.bas`*

```basic
TYPE Inner
    X AS DOUBLE
END TYPE
```

### `Int2`

*definizione da `shared/minitests/ffi/ffi_udt_struct.bas`*

```basic
TYPE Int2
    A AS LONG
    B AS LONG
END TYPE
```

```basic
31:    DIM i1 AS Int2
```

### `Int3`

*definizione da `shared/minitests/ffi/ffi_udt_multi.bas`*

```basic
TYPE Int3
    A AS LONG
    B AS LONG
    C AS LONG
END TYPE
```

```basic
60:    DIM v AS Int3
```

### `Item`

*definizione da `shared/minitests/dbf/isam_replication.bas`*

```basic
TYPE Item
    ID AS STRING * 8
    Name AS STRING * 30
END TYPE
```

```basic
31:    DIM R AS Item
```

### `Labeled`

*definizione da `shared/samples/FFI/NativeVec/NativeVec.bas`*

```basic
TYPE Labeled
    V AS DOUBLE
    N AS LONG
END TYPE
```

```basic
118:    DIM lab AS Labeled
```

### `LabeledDN`

*definizione da `shared/minitests/ffi/ffi_udt_mixed_kind_return.bas`*

```basic
TYPE LabeledDN
    V AS DOUBLE
    N AS LONG
END TYPE
```

```basic
16:    DIM a AS LabeledDN
```

### `LabeledND`

*definizione da `shared/minitests/ffi/ffi_udt_mixed_kind_return.bas`*

```basic
TYPE LabeledND
    N AS LONG
    V AS DOUBLE
END TYPE
```

```basic
21:    DIM b AS LabeledND
```

### `LedgerBalanceEntry`

*definizione da `shared/samples/Databases/DbLedgerModernTests.bas`*

```basic
    TYPE LedgerBalanceEntry
        Account AS STRING * 80
        TotalAmount AS CURRENCY
    END TYPE
```

### `LedgerEntryDetail`

*definizione da `shared/samples/Databases/DbLedgerModernTests.bas`*

```basic
    TYPE LedgerEntryDetail
        TransactionId AS LONG
        Date AS DATE
        Status AS STRING * 1
        Description AS STRING * 100
        Account AS STRING * 80
        Amount AS CURRENCY
        Comment AS STRING * 100
        AmountIsExplicit AS BIT
        Currency AS STRING * 3
        ExchangeRate AS CURRENCY
        BaseAmount AS CURRENCY
    END TYPE
```

### `Leg`

*definizione da `shared/minitests/types/udt_list_field.bas`*

```basic
TYPE Leg
  side AS STRING * 6
  strike AS DOUBLE
END TYPE
```

```basic
14:  DIM a AS Leg
```

### `Level1`

*definizione da `shared/minitests/types/nested_udt_field.bas`*

```basic
TYPE Level1
    A AS Level2
END TYPE
```

```basic
27:    DIM t AS Level1
```

### `Level2`

*definizione da `shared/minitests/types/nested_udt_field.bas`*

```basic
TYPE Level2
    B AS Level3
END TYPE
```

### `Level3`

*definizione da `shared/minitests/types/nested_udt_field.bas`*

```basic
TYPE Level3
    C AS DOUBLE
END TYPE
```

### `Linea`

*definizione da `shared/samples/System/SystemSizeOfUdtLayout.bas`*

```basic
TYPE Linea
  id AS LONG
  label AS STRING * 8
  amount AS DOUBLE
END TYPE
```

### `LockRec`

*definizione da `shared/samples/Databases/RDD_Complete.bas`*

```basic
TYPE LockRec
    ID AS STRING * 8
    Val AS LONG
END TYPE
```

```basic
65:DIM lr AS LockRec
```

### `LockT`

*definizione da `shared/samples/Databases/RDD_Legacy.bas`*

```basic
TYPE LockT
    ID AS STRING * 8
    Val AS LONG
END TYPE
```

```basic
35:DIM lt AS LockT
```

### `loop`

*definizione da `shared/minitests/system/keyword_names.bas`*

```basic
TYPE loop
  for AS INTEGER
END TYPE
```

```basic
36:  DIM v AS loop
```

### `M16`

*definizione da `shared/minitests/ffi/ffi_udt_double_after_udts.bas`*

```basic
TYPE M16
    K AS QUAD
    V AS DOUBLE
END TYPE
```

```basic
19:    DIM m AS M16
```

### `Misto`

*definizione da `shared/minitests/io/random_udt_record.bas`*

```basic
TYPE Misto
  B AS BYTE
  I AS INTEGER
  S AS SINGLE
  D AS DOUBLE
  Q AS QUAD
  T AS STRING * 3
END TYPE
```

```basic
27:  DIM m AS Misto, n AS Misto
```

### `Mixed`

*definizione da `shared/minitests/ffi/ffi_udt_layout.bas`*

```basic
TYPE Mixed
    A AS LONG
    B AS DOUBLE
    C AS BYTE
END TYPE
```

```basic
48:    DIM m AS Mixed
```

### `Mixed3`

*definizione da `shared/minitests/ffi/ffi_udt_multi.bas`*

```basic
TYPE Mixed3
    A AS DOUBLE
    B AS LONG
    C AS DOUBLE
END TYPE
```

```basic
36:    DIM m AS Mixed3
```

### `MixT`

*definizione da `shared/samples/Databases/RDD_Mixed_Demo.bas`*

```basic
TYPE MixT
    ID AS STRING * 8
    Val AS LONG
END TYPE
```

```basic
25:DIM m AS MixT
```

### `MyPackedStruct`

*definizione da `shared/samples/System/SystemLowLevel.bas`*

```basic
TYPE MyPackedStruct
    B1 AS BYTE
    L1 AS LONG
    B2 AS BYTE
END TYPE
```

```basic
68:    DIM p AS MyPackedStruct
```

### `MySimpleStruct`

*definizione da `shared/samples/System/SystemLowLevel.bas`*

```basic
TYPE MySimpleStruct
    ID AS INTEGER
    Value AS DOUBLE
    Flag AS BIT
END TYPE
```

```basic
46:    DIM u AS MySimpleStruct
```

### `MyUdt`

*definizione da `shared/samples/ParserStress.bas`*

```basic
    TYPE MyUdt
        items(5) AS STRING * 10
    END TYPE
```

```basic
116:    DIM u AS MyUdt
```

### `N2`

*definizione da `shared/minitests/system/system_sizeof_layout.bas`*

```basic
TYPE N2
  a AS INTEGER
  b AS INTEGER
END TYPE
```

### `Named2`

*definizione da `shared/minitests/ffi/ffi_udt_layout.bas`*

```basic
TYPE Named2
    S AS STRING
    N AS LONG
END TYPE
```

```basic
67:    DIM nm AS Named2
```

### `NamedVec`

*definizione da `shared/minitests/ffi/ffi_udt_string_fieldarray.bas`*

```basic
TYPE NamedVec
    NAME AS STRING
    V(2) AS LONG
END TYPE
```

```basic
14:    DIM n AS NamedVec
```

### `NestPacked`

*definizione da `shared/minitests/system/system_sizeof_layout.bas`*

```basic
TYPE NestPacked
  p AS P2
  d AS DOUBLE
END TYPE
```

```basic
82:  DIM np AS NestPacked : PRINT SIZEOF(np)
```

### `Oct8`

*definizione da `shared/minitests/ffi/ffi_udt_multi.bas`*

```basic
TYPE Oct8
    F0 AS DOUBLE
    F1 AS DOUBLE
    F2 AS DOUBLE
    F3 AS DOUBLE
    F4 AS DOUBLE
    F5 AS DOUBLE
    F6 AS DOUBLE
    F7 AS DOUBLE
END TYPE
```

```basic
50:    DIM o AS Oct8
```

### `Ord`

*definizione da `shared/samples/Databases/DbfRelationJoin.bas`*

```basic
TYPE Ord
    CID AS STRING * 10
    ITEM AS STRING * 10
END TYPE
```

### `Ordine`

*definizione da `shared/minitests/speed/select.bas`*

```basic
TYPE Ordine
  ID_Cliente AS LONG
  ID_Ordine AS LONG
  Descrizione AS STRING * 50
  Importo AS CURRENCY
END TYPE
```

```basic
42:  DIM o AS Ordine
```

### `OrdT`

*definizione da `shared/samples/Databases/RDD_Relation.bas`*

```basic
TYPE OrdT
    OID AS STRING * 8
    CustID AS STRING * 8
    Amount AS DOUBLE
END TYPE
```

```basic
34:DIM o AS OrdT
```

### `Outer`

*definizione da `shared/minitests/types/nested_udt_field.bas`*

```basic
TYPE Outer
    I AS Inner
    Y AS LONG
END TYPE
```

```basic
22:    DIM o AS Outer
```

### `OuterOfPacked`

*definizione da `shared/minitests/system/system_sizeof_forms.bas`*

```basic
TYPE OuterOfPacked
  p AS InnerPacked
  b AS BYTE
END TYPE
```

```basic
65:  DIM oq AS OuterOfPacked : PRINT SYSTEM.UDTPACKED(oq)
```

### `OuterPlain`

*definizione da `shared/minitests/system/system_sizeof_forms.bas`*

```basic
TYPE OuterPlain
  i AS Inner
  d AS DOUBLE
END TYPE
```

```basic
76:  DIM op AS OuterPlain : PRINT SYSTEM.UDTPACKED(op)
```

### `P`

*definizione da `shared/minitests/types/udt_keyword_member.bas`*

```basic
TYPE P
  volume AS DOUBLE
  qty AS DOUBLE
END TYPE
```

```basic
12:  DIM p AS P
```

### `Pair`

*definizione da `shared/samples/Types/TypesGenericType.bas`*

```basic
TYPE Pair<T1, T2>
    First AS T1
    Second AS T2
END TYPE
```

```basic
37:DIM p AS Pair<INTEGER, STRING>
```

### `Person`

*definizione da `shared/minitests/system/system_typeinfo_has.bas`*

```basic
TYPE Person
    Nome AS STRING * 10
    Eta AS INTEGER
END TYPE
```

```basic
14:    DIM u AS Person
```

### `Persona`

*definizione da `shared/samples/Types/TypesUdtGlobal.bas`*

```basic
TYPE Persona
  Nome AS STRING * 15
  Cognome AS STRING * 20
  Eta AS INTEGER
  Attivo AS BIT
END TYPE
```

```basic
40:  DIM P AS Persona
```

### `Point`

*definizione da `shared/minitests/ffi/ffi_udt_struct.bas`*

```basic
TYPE Point
    X AS DOUBLE
    Y AS DOUBLE
END TYPE
```

```basic
18:    DIM p1 AS Point
```

### `Point2D`

*definizione da `shared/samples/graphics/GraphicsDrawTextures.bas`*

```basic
TYPE Point2D
    X AS DOUBLE
    Y AS DOUBLE
END TYPE
```

### `Points`

*definizione da `shared/minitests/system/system_sizeof_static.bas`*

```basic
TYPE Points
  x AS INTEGER
  y AS INTEGER
END TYPE
```

### `PRec`

*definizione da `shared/minitests/dbf/isam_filter_eof.bas`*

```basic
TYPE PRec
    Code AS STRING * 8
    Price AS DOUBLE
    Qty AS LONG
END TYPE
```

```basic
16:DIM p AS PRec
```

### `Prodotto`

*definizione da `shared/minitests/arrays/udt_higher_order.bas`*

```basic
TYPE Prodotto
    Nome AS STRING * 20
    Prezzo AS DOUBLE
END TYPE
```

### `Product`

*definizione da `shared/samples/Databases/RDD_Complete.bas`*

```basic
TYPE Product
    Code AS STRING * 8
    Name AS STRING * 32
    Price AS DOUBLE
    Qty AS LONG
END TYPE
```

```basic
33:DIM p AS Product
```

### `Pt2`

*definizione da `shared/minitests/ffi/ffi_udt_combo.bas`*

```basic
TYPE Pt2
    X AS DOUBLE
    Y AS DOUBLE
END TYPE
```

### `Punto`

*definizione da `shared/minitests/arrays/local_array_udt_2d.bas`*

```basic
TYPE Punto
  x AS INTEGER
  y AS INTEGER
END TYPE
```

### `R`

*definizione da `shared/minitests/types/class_array_member_udt.bas`*

```basic
TYPE R
  campo AS INTEGER
  altro AS INTEGER
END TYPE
```

### `Rec`

*definizione da `shared/minitests/system/system_sizeof_layout.bas`*

```basic
TYPE Rec
  id AS LONG
  name AS STRING * 10
  v AS DOUBLE
END TYPE
```

```basic
84:  DIM rc AS Rec : PRINT SIZEOF(rc)
```

### `Record`

*definizione da `shared/samples/System/SystemSizeOfUdtLayout.bas`*

```basic
TYPE Record
  h AS Header
  l AS Linea
  total AS CURRENCY
  tag AS STRING * 4
END TYPE
```

```basic
130:  DIM r AS Record
```

### `Rettangolo`

*definizione da `shared/minitests/constructs/types_interface.bas`*

```basic
TYPE Rettangolo IMPLEMENTS IShape
    Larghezza AS DOUBLE
    Altezza AS DOUBLE
    FUNCTION Area() AS DOUBLE
        AREA = SELF.Larghezza * SELF.Altezza
    END FUNCTION
    FUNCTION Describe() AS STRING
        DESCRIBE = "Rect"
    END FUNCTION
END TYPE
```

```basic
31:    DIM r AS Rettangolo
```

### `Riga`

*definizione da `shared/minitests/speed/validate.bas`*

```basic
TYPE Riga
  ID AS LONG
  Nome AS STRING * 16
END TYPE
```

### `RisultatoJoin`

*definizione da `shared/minitests/speed/select.bas`*

```basic
TYPE RisultatoJoin
  NomeCliente AS STRING * 50
  TotaleImporto AS CURRENCY
END TYPE
```

### `Row`

*definizione da `shared/samples/Databases/DbDbfGroupCommit.bas`*

```basic
TYPE Row
  Name AS STRING * 20
END TYPE
```

### `Sale`

*definizione da `shared/minitests/dbf/bare_currency_y_print_str.bas`*

```basic
TYPE Sale
    Code   AS STRING * 10
    Amount AS CURRENCY
END TYPE
```

### `Sample`

*definizione da `shared/samples/Arrays/MultidimBounds.bas`*

```basic
TYPE Sample
    Value AS DOUBLE
    Label AS STRING
END TYPE
```

### `SRec`

*definizione da `shared/minitests/dbf/isam_single_roundtrip.bas`*

```basic
TYPE SRec
    Code AS STRING * 8
    Ratio AS SINGLE
END TYPE
```

```basic
10:DIM p AS SRec
```

### `SRow`

*definizione da `shared/minitests/speed/setmany.bas`*

```basic
TYPE SRow
  ID AS LONG
  Name AS STRING * 32
END TYPE
```

```basic
22:  DIM r1 AS SRow
```

### `Stazione`

*definizione da `shared/minitests/types/udt_nested_2level.bas`*

```basic
TYPE Stazione
  pos AS Coord
  codice AS STRING * 6
END TYPE
```

```basic
14:  PUBLIC DIM stazione AS Stazione
```

### `T`

*definizione da `shared/minitests/types/udt_constructor.bas`*

```basic
TYPE T
  X AS LONG
  Y AS LONG
  SUB NEW(a AS LONG, b AS LONG)
    SELF.X = a
    SELF.Y = b
  END SUB
  FUNCTION Somma() AS LONG
    FUNCTION = SELF.X + SELF.Y
  END FUNCTION
END TYPE
```

```basic
18:  DIM t AS T = NEW T(7, 9)
```

### `TA`

*definizione da `shared/minitests/dbf/mt41_append_for.bas`*

```basic
TYPE TA
    Saldo AS INTEGER
END TYPE
```

### `TAddetti`

*definizione da `shared/samples/Databases/RDD_Buffering_Complete.bas`*

```basic
TYPE TAddetti
    CODICE AS STRING * 8
    NOME   AS STRING * 20
    STIPENDIO AS DOUBLE
    UFFICIO AS STRING * 8
END TYPE
```

```basic
33:    DIM a AS TAddetti
```

### `TArea`

*definizione da `shared/minitests/dbf/dbf_selected_area.bas`*

```basic
TYPE TArea
    K AS STRING * 3
END TYPE
```

### `TB`

*definizione da `shared/minitests/dbf/dbf_select.bas`*

```basic
TYPE TB
    K AS STRING * 3
END TYPE
```

### `TBit`

*definizione da `shared/minitests/dbf/dbf_bit_replace.bas`*

```basic
TYPE TBit
    ID AS INTEGER
    Attivo AS BIT
END TYPE
```

### `TBuf`

*definizione da `shared/minitests/dbf/rdd_buffer.bas`*

```basic
TYPE TBuf
    ID AS STRING * 8
    Nome AS STRING * 32
    Sal AS DOUBLE
    Dip AS STRING * 16
END TYPE
```

```basic
24:    DIM e AS TBuf
```

### `TC`

*definizione da `shared/minitests/dbf/mt41_index_composite.bas`*

```basic
TYPE TC
    Citta AS STRING * 20
    Nome AS STRING * 20
END TYPE
```

### `TCli`

*definizione da `shared/minitests/dbf/dbf_export_import.bas`*

```basic
TYPE TCli
  ID AS INTEGER
  Nome AS STRING * 10
END TYPE
```

### `TCLI`

*definizione da `shared/minitests/dbf/dbf_join_multimatch.bas`*

```basic
TYPE TCLI
    ID AS STRING * 5
    NOME AS STRING * 10
END TYPE
```

### `Tcliente`

*definizione da `shared/minitests/dbf/isam_protocol41.bas`*

```basic
TYPE Tcliente
    ID AS LONG
    Nome AS STRING * 20
    Saldo AS CURRENCY
    Attivo AS BIT
END TYPE
```

```basic
17:    DIM r AS Tcliente
```

### `TCliente`

*definizione da `shared/samples/Databases/AnagraficaClienti.bas`*

```basic
TYPE TCliente
    Nome AS STRING * 20
    Citta AS STRING * 15
    Saldo AS DOUBLE
END TYPE
```

```basic
25:    DIM r AS TCliente
```

### `TClienti`

*definizione da `shared/samples/Databases/SelectAreas.bas`*

```basic
TYPE TClienti
    ID AS STRING * 4
    NOME AS STRING * 12
END TYPE
```

### `TD`

*definizione da `shared/minitests/dbf/mt41_delete_for.bas`*

```basic
TYPE TD
    ID AS INTEGER
    Saldo AS INTEGER
END TYPE
```

### `TestRecord`

*definizione da `shared/samples/Files/FilesLocking.bas`*

```basic
TYPE TestRecord
    ID AS LONG
    Nome AS STRING * 30
END TYPE
```

```basic
52:    DIM rec AS TestRecord
```

### `TestUDT`

*definizione da `shared/minitests/system/system_udtaligned.bas`*

```basic
TYPE TestUDT
  a AS INTEGER
  b AS DOUBLE
END TYPE
```

```basic
9:  DIM u AS TestUDT
```

### `TF`

*definizione da `shared/minitests/dbf/dbf_file_ops_for.bas`*

```basic
TYPE TF
    ID AS INTEGER
    Nome AS STRING * 20
    Citta AS STRING * 20
    Saldo AS CURRENCY
    Attivo AS BIT
END TYPE
```

### `TGet`

*definizione da `shared/minitests/dbf/dbf_get_var.bas`*

```basic
TYPE TGet
    ID AS INTEGER
    Nome AS STRING * 20
END TYPE
```

### `Ticket`

*definizione da `shared/minitests/types/udt_list_field.bas`*

```basic
TYPE Ticket
  name AS STRING * 8
  legs AS LIST OF Leg
END TYPE
```

```basic
12:  DIM t AS Ticket
```

### `TIx`

*definizione da `shared/minitests/dbf/dbf_index_expr.bas`*

```basic
TYPE TIx
    ID AS INTEGER
    Nome AS STRING * 20
    Citta AS STRING * 20
    Saldo AS CURRENCY
END TYPE
```

### `TL`

*definizione da `shared/minitests/dbf/mt41_list_for.bas`*

```basic
TYPE TL
    ID AS INTEGER
    Citta AS STRING * 20
END TYPE
```

### `TM`

*definizione da `shared/minitests/dbf/mt41_join_lambda.bas`*

```basic
TYPE TM
    Rif AS INTEGER
    Imp AS INTEGER
END TYPE
```

### `TMisc`

*definizione da `shared/minitests/dbf/dbf_lock_filter.bas`*

```basic
TYPE TMisc
    ID AS STRING * 10
    NOME AS STRING * 10
END TYPE
```

### `TORD`

*definizione da `shared/minitests/dbf/dbf_join_multimatch.bas`*

```basic
TYPE TORD
    ID AS STRING * 5
    IDCLI AS STRING * 5
END TYPE
```

### `TOrdini`

*definizione da `shared/samples/Databases/SelectAreas.bas`*

```basic
TYPE TOrdini
    CID AS STRING * 4
    DESCR AS STRING * 12
END TYPE
```

### `TPROD`

*definizione da `shared/samples/Databases/DbfJoinSelectAdvanced.bas`*

```basic
TYPE TPROD
    COD AS STRING * 5
    PREZZO AS DOUBLE
END TYPE
```

### `TradeResult`

*definizione da `shared/minitests/json/toobject_udt_fn.bas`*

```basic
TYPE TradeResult
  retcode AS DOUBLE
  order AS DOUBLE
  deal AS DOUBLE
  price AS DOUBLE
END TYPE
```

```basic
22:  DIM res AS TradeResult
```

### `TRdd`

*definizione da `shared/minitests/dbf/rdd_fields.bas`*

```basic
TYPE TRdd
    ID AS STRING * 10
    NOME AS STRING * 10
END TYPE
```

### `TRddCov`

*definizione da `shared/samples/Databases/RddCompleteCoverage.bas`*

```basic
TYPE TRddCov
    ID AS STRING * 10
    NOME AS STRING * 10
    VAL AS DOUBLE
END TYPE
```

### `TRddCur`

*definizione da `shared/minitests/dbf/rdd_cursor_dates.bas`*

```basic
TYPE TRddCur
    ID AS STRING * 10
    NOME AS STRING * 10
END TYPE
```

### `TRec`

*definizione da `shared/minitests/dbf/replication_tcp.bas`*

```basic
TYPE TRec
    Name AS STRING * 20
END TYPE
```

### `TReport`

*definizione da `shared/minitests/dbf/dbf_all.bas`*

```basic
TYPE TReport
  ID AS INTEGER
  Nome AS STRING * 10
END TYPE
```

### `TRow`

*definizione da `shared/minitests/dbf/dbf_replication.bas`*

```basic
TYPE TRow
    Name AS STRING * 20
END TYPE
```

```basic
42:    DIM r AS TRow
```

### `TS`

*definizione da `shared/minitests/dbf/mt41_sort_desc_for.bas`*

```basic
TYPE TS
    Nome AS STRING * 10
    Saldo AS INTEGER
    Attivo AS INTEGER
END TYPE
```

### `TSeek`

*definizione da `shared/minitests/dbf/dbf_seek_order.bas`*

```basic
TYPE TSeek
    ID AS INTEGER
    Nome AS STRING * 10
END TYPE
```

### `TStato`

*definizione da `shared/minitests/dbf/dbf_state.bas`*

```basic
TYPE TStato
    ID AS STRING * 10
    NOME AS STRING * 10
END TYPE
```

### `TT`

*definizione da `shared/minitests/dbf/mt41_total_for.bas`*

```basic
TYPE TT
    Citta AS STRING * 20
    Saldo AS INTEGER
    Attivo AS INTEGER
END TYPE
```

### `TT2`

*definizione da `shared/minitests/dbf/rdd_buffer_replace.bas`*

```basic
TYPE TT2
    ID AS STRING * 10
    NOME AS STRING * 10
    SAL AS DOUBLE
END TYPE
```

### `TUpd`

*definizione da `shared/minitests/dbf/update_on.bas`*

```basic
TYPE TUpd
    ID AS INTEGER
    Nome AS STRING * 20
    Saldo AS CURRENCY
END TYPE
```

### `TZ`

*definizione da `shared/minitests/dbf/mt41_close_all.bas`*

```basic
TYPE TZ
    ID AS INTEGER
END TYPE
```

### `UnpackedPoint`

*definizione da `shared/samples/Types/TypesPackedUdt.bas`*

```basic
TYPE UnpackedPoint
    x AS INTEGER
    y AS INTEGER
END TYPE
```

```basic
32:DIM up AS UnpackedPoint
```

### `UnpackedRecord`

*definizione da `shared/minitests/system/system_sizeof_static.bas`*

```basic
TYPE UnpackedRecord
  id AS LONG
  flag AS BIT
  value AS DOUBLE
END TYPE
```

### `Utente`

*definizione da `shared/samples/Databases/ISAM_Simple_Test.bas`*

```basic
TYPE Utente
    ID AS STRING * 10
    Nome AS STRING * 50
    Eta AS INTEGER
END TYPE
```

```basic
18:DIM rec AS Utente
```

### `UUns`

*definizione da `shared/minitests/ffi/ffi_udt_unsigned.bas`*

```basic
TYPE UUns
    U16 AS UINTEGER
    U32 AS ULONG
    U64 AS UQUAD
END TYPE
```

```basic
14:    DIM v AS UUns
```

### `Vector2D`

*definizione da `shared/samples/Classes/ClassesUdtAdvances.bas`*

```basic
TYPE Vector2D
    ' --- CAMPI ---
    ' I campi contengono i dati dell'istanza.
    X AS DOUBLE
    Y AS DOUBLE

    ' --- COSTRUTTORE ---
    ' Un costruttore speciale 'SUB NEW' che viene chiamato quando si crea un'istanza con NEW.
    SUB NEW(x_val AS DOUBLE, y_val AS DOUBLE)
        ' 'SELF' si riferisce all'istanza corrente dell'UDT.
        SELF.X = x_val
        SELF.Y = y_val
    END SUB

    ' --- PROPRIETÀ ---
    ' Una proprietà calcolata di sola lettura (ha solo un blocco GET).
    PUBLIC PROPERTY Magnitude AS DOUBLE
        GET
            ' Calcola e restituisce la lunghezza del vettore usando il teorema di Pitagora.
            ' Utilizza la funzione SQR dal namespace MATH importato.
            RETURN SQR(SELF.X ^ 2 + SELF.Y ^ 2)
        END GET
    END PROPERTY

    ' --- METODI ---
    ' Un metodo SUB che modifica lo stato interno dell'istanza.
    PUBLIC SUB Normalize()
        DIM mag AS DOUBLE = SELF.Magnitude ' Legge la proprietà calcolata.
        IF mag > 0 THEN
            ' Divide le componenti per la magnitudo per ottenere un vettore di lunghezza 1.
```

```basic
121:    DIM v1 AS Vector2D = NEW Vector2D(3, 4)
```

### `Vendita`

*definizione da `shared/samples/Arrays/ArraysComplexProcessing.bas`*

```basic
TYPE Vendita
    ID AS LONG
    Prodotto AS STRING * 30
    Quantita AS INTEGER
    PrezzoUnitario AS CURRENCY
    Annullato AS BOOLEAN
END TYPE
```

```basic
128:    DIM v AS Vendita
```

### `W`

*definizione da `shared/minitests/types/udt_keyword_member.bas`*

```basic
TYPE W
  n AS DOUBLE
  legs AS LIST OF P
END TYPE
```

```basic
21:  DIM w AS W
```

### `Wide`

*definizione da `shared/minitests/ffi/ffi_udt_layout.bas`*

```basic
TYPE Wide
    A AS UINTEGER
    B AS ULONG
    C AS UQUAD
END TYPE
```

```basic
71:    DIM w AS Wide
```

### `Wide16`

*definizione da `shared/minitests/ffi/ffi_udt_wide16.bas`*

```basic
TYPE Wide16
    A AS INTEGER
    B AS LONG
    C AS DOUBLE
    D AS SINGLE
    E AS QUAD
    F AS BYTE
    G AS DOUBLE
    H AS LONG
    I AS DOUBLE
    J AS INTEGER
    K AS DOUBLE
    L AS LONG
    M AS DOUBLE
    N AS QUAD
    O AS DOUBLE
    P AS SINGLE
END TYPE
```

```basic
27:    DIM w AS Wide16
```

### `Wide64`

*definizione da `shared/minitests/ffi/ffi_udt_wide64.bas`*

```basic
TYPE Wide64
    F0 AS LONG
    F1 AS LONG
    F2 AS LONG
    F3 AS LONG
    F4 AS LONG
    F5 AS LONG
    F6 AS LONG
    F7 AS LONG
    F8 AS LONG
    F9 AS LONG
    F10 AS LONG
    F11 AS LONG
    F12 AS LONG
    F13 AS LONG
    F14 AS LONG
    F15 AS LONG
    F16 AS LONG
    F17 AS LONG
    F18 AS LONG
    F19 AS LONG
    F20 AS LONG
    F21 AS LONG
    F22 AS LONG
    F23 AS LONG
    F24 AS LONG
    F25 AS LONG
    F26 AS LONG
    F27 AS LONG
    F28 AS LONG
```

```basic
78:    DIM w AS Wide64
```

### `WithArray`

*definizione da `shared/minitests/system/system_sizeof_layout.bas`*

```basic
TYPE WithArray
  arr(4) AS LONG
  n AS INTEGER
END TYPE
```

```basic
83:  DIM wa AS WithArray : PRINT SIZEOF(wa)
```

