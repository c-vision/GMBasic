// FILE: StdLib.Constants.cs
// Costanti stile PowerBASIC (%NOME): sempre disponibili senza alcun IMPORT, registrate sotto lo
// pseudo-namespace "CONSTANTS" (vedi il seed namespacesToConsider/nsSet in Executor/Orchestrator.cs
// e Pipeline/SourcePipeline.cs — stesso trattamento già riservato a SYSTEM/WINDOW).
//
// Sostituiscono gli ex-namespace di sole costanti COLOR/BUTTON/BORDER (rimossi per intero, incluso
// il loro stub COUNT mai wired ad alcun dispatcher) e le ex-costanti JOY.*/IO.FILE_*/KEY.* — le
// funzioni VERE dei namespace JOY (JOY/JOYCOUNT/JOYNAME/JOYPRESENT) e IO (~25 funzioni file I/O)
// restano intatte nei rispettivi file, solo le loro costanti si sono spostate qui.
//
// I valori KEY.* sono invariati rispetto al vecchio dizionario hardcoded
// (ex Compiler/Compiler.Constants.cs::KeyConstants, ora rimosso): le combinazioni CTRL_C/
// CTRL_BREAK/SHIFT_TAB restano -1 come placeholder, il valore reale viene costruito a runtime via
// KEY.MAKE — comportamento invariato dalla migrazione.

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Constants — namespace 'CONSTANTS'
 * 0 funzioni | 0 comandi | 0 tipi | 284 costanti
 *
 * COSTANTI (284):
 *   %ABORT
 *   %ABORTRETRYIGNORE
 *   %ANSI
 *   %AQUA
 *   %AQUAMARINE1
 *   %AQUAMARINE1_2
 *   %AQUAMARINE3
 *   %AUDIO
 *   %AUTO
 *   %BLACK
 *   %BLUE
 *   %BLUE1
 *   %BLUE3
 *   %BLUE3_2
 *   %BLUE_VIOLET
 *   %C64
 *   %CADET_BLUE
 *   %CADET_BLUE_2
 *   %CANCEL
 *   %CENTER
 *   %CHARTREUSE1
 *   %CHARTREUSE2
 *   %CHARTREUSE2_2
 *   %CHARTREUSE3
 *   %CHARTREUSE3_2
 *   %CHARTREUSE4
 *   %CHAT
 *   %CHCLOSED
 *   %CHROMIUM
 *   %CLOSED
 *   %CORNFLOWER_BLUE
 *   %CYAN1
 *   %CYAN2
 *   %CYAN3
 *   %DARK_BLUE
 *   %DARK_CYAN
 *   %DARK_GREEN
 *   %DARK_MAGENTA
 *   %DARK_MAGENTA_2
 *   %DARK_OLIVE_GREEN3
 *   %DARK_OLIVE_GREEN3_2
 *   %DARK_RED
 *   %DARK_RED_2
 *   %DARK_SEA_GREEN
 *   %DARK_SEA_GREEN3
 *   %DARK_SEA_GREEN4
 *   %DARK_SEA_GREEN4_2
 *   %DARK_SLATE_GRAY1
 *   %DARK_SLATE_GRAY2
 *   %DARK_SLATE_GRAY3
 *   %DARK_TURQUOISE
 *   %DARK_VIOLET
 *   %DEEP_PINK4
 *   %DEEP_PINK4_2
 *   %DEEP_SKY_BLUE1
 *   %DEEP_SKY_BLUE2
 *   %DEEP_SKY_BLUE3
 *   %DEEP_SKY_BLUE3_2
 *   %DEEP_SKY_BLUE4
 *   %DEEP_SKY_BLUE4_2
 *   %DEEP_SKY_BLUE4_3
 *   %DODGER_BLUE1
 *   %DODGER_BLUE2
 *   %DODGER_BLUE3
 *   %DOUBLE
 *   %EMBEDDING
 *   %FAST
 *   %FILE_CHANGED
 *   %FILE_CREATED
 *   %FILE_DELETED
 *   %FILE_RENAMED
 *   %FUCHSIA
 *   %GOVERNED
 *   %GRAY
 *   %GRAY0
 *   %GRAY11
 *   %GRAY15
 *   %GRAY19
 *   %GRAY2
 *   %GRAY23
 *   %GRAY27
 *   %GRAY3
 *   %GRAY30
 *   %GRAY35
 *   %GRAY37
 *   %GRAY39
 *   %GRAY42
 *   %GRAY46
 *   %GRAY50
 *   %GRAY53
 *   %GRAY54
 *   %GRAY58
 *   %GRAY62
 *   %GRAY66
 *   %GRAY7
 *   %GRAY70
 *   %GRAY74
 *   %GRAY78
 *   %GRAY82
 *   %GRAY85
 *   %GRAY89
 *   %GREEN
 *   %GREEN1
 *   %GREEN3
 *   %GREEN3_2
 *   %GREEN4
 *   %IGNORE
 *   %JOYBUTTON2
 *   %JOYDOWN
 *   %JOYFIRE
 *   %JOYLEFT
 *   %JOYRIGHT
 *   %JOYUP
 *   %K_0
 *   %K_1
 *   %K_2
 *   %K_3
 *   %K_4
 *   %K_5
 *   %K_6
 *   %K_7
 *   %K_8
 *   %K_9
 *   %K_A
 *   %K_ALT
 *   %K_B
 *   %K_BS
 *   %K_C
 *   %K_CTRL
 *   %K_CTRL_BRK
 *   %K_CTRL_C
 *   %K_D
 *   %K_DEL
 *   %K_DOWN
 *   %K_E
 *   %K_END
 *   %K_ENTER
 *   %K_ESC
 *   %K_F
 *   %K_F1
 *   %K_F10
 *   %K_F11
 *   %K_F12
 *   %K_F2
 *   %K_F3
 *   %K_F4
 *   %K_F5
 *   %K_F6
 *   %K_F7
 *   %K_F8
 *   %K_F9
 *   %K_G
 *   %K_H
 *   %K_HOME
 *   %K_I
 *   %K_INS
 *   %K_J
 *   %K_K
 *   %K_L
 *   %K_LEFT
 *   %K_M
 *   %K_N
 *   %K_N0
 *   %K_N1
 *   %K_N2
 *   %K_N3
 *   %K_N4
 *   %K_N5
 *   %K_N6
 *   %K_N7
 *   %K_N8
 *   %K_N9
 *   %K_NADD
 *   %K_NDEC
 *   %K_NDIV
 *   %K_NMUL
 *   %K_NSUB
 *   %K_O
 *   %K_P
 *   %K_PAUSE
 *   %K_PGDN
 *   %K_PGUP
 *   %K_PRTSC
 *   %K_Q
 *   %K_R
 *   %K_RIGHT
 *   %K_S
 *   %K_SHIFT
 *   %K_SHIFT_TAB
 *   %K_SPC
 *   %K_T
 *   %K_TAB
 *   %K_U
 *   %K_UP
 *   %K_V
 *   %K_W
 *   %K_X
 *   %K_Y
 *   %K_Z
 *   %LEFT
 *   %LIGHT_GREEN
 *   %LIGHT_GREEN_2
 *   %LIGHT_PINK4
 *   %LIGHT_SEA_GREEN
 *   %LIGHT_SKY_BLUE3
 *   %LIGHT_SKY_BLUE3_2
 *   %LIGHT_SLATE_BLUE
 *   %LIGHT_SLATE_GRAY
 *   %LIME
 *   %MAROON
 *   %MEDIUM_PURPLE
 *   %MEDIUM_PURPLE3
 *   %MEDIUM_PURPLE3_2
 *   %MEDIUM_PURPLE4
 *   %MEDIUM_SPRING_GREEN
 *   %MEDIUM_TURQUOISE
 *   %NAVY
 *   %NAVY_BLUE
 *   %NO
 *   %NONE
 *   %OBSCURA
 *   %OCR
 *   %OK
 *   %OKCANCEL
 *   %OLIVE
 *   %ORANGE4
 *   %ORANGE4_2
 *   %PALE_GREEN1
 *   %PALE_GREEN3
 *   %PALE_GREEN3_2
 *   %PALE_TURQUOISE4
 *   %PLUM4
 *   %PURPLE
 *   %PURPLE1
 *   %PURPLE3
 *   %PURPLE4
 *   %PURPLE4_2
 *   %RECEIVED
 *   %RED
 *   %RERANKER
 *   %RERANK_KEYWORD
 *   %RERANK_OFF
 *   %RERANK_ONNX
 *   %RETRY
 *   %RETRYCANCEL
 *   %RIGHT
 *   %ROUNDED
 *   %ROYAL_BLUE1
 *   %SDL
 *   %SEA_GREEN1
 *   %SEA_GREEN1_2
 *   %SEA_GREEN2
 *   %SEA_GREEN3
 *   %SILVER
 *   %SINGLE
 *   %SKY_BLUE1
 *   %SKY_BLUE2
 *   %SKY_BLUE3
 *   %SLATE_BLUE1
 *   %SLATE_BLUE3
 *   %SLATE_BLUE3_2
 *   %SPRING_GREEN1
 *   %SPRING_GREEN2
 *   %SPRING_GREEN2_2
 *   %SPRING_GREEN3
 *   %SPRING_GREEN3_2
 *   %SPRING_GREEN4
 *   %STEEL_BLUE
 *   %STEEL_BLUE1
 *   %STEEL_BLUE1_2
 *   %STEEL_BLUE3
 *   %SYSTEM
 *   %TEAL
 *   %TTS
 *   %TURQUOISE2
 *   %TURQUOISE4
 *   %WHEAT4
 *   %WHITE
 *   %YELLOW
 *   %YELLOW4
 *   %YELLOW4_2
 *   %YES
 *   %YESNO
 *   %YESNOCANCEL
    ==== GMB-STDLIB-AUDIT-END ==== */


using System;
using System.Collections.Generic;
using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateConstantsNamespace()
    {
        const string namespaceName = "CONSTANTS";
        var constants = new List<ConstDeclaration>();

        void AddConst(string name, double value)
        {
            constants.Add(new ConstDeclaration(-1,
                T(TokenKind.Identifier, "%" + name),
                new IntegerType(),
                new NumberExpr(value, T(TokenKind.Number, value.ToString(System.Globalization.CultureInfo.InvariantCulture))),
                SymbolFlags.IsPublic | SymbolFlags.IsConst));
        }

        // Allineamento testo box (ALERT/CONFIRM): %LEFT=0 %CENTER=1 %RIGHT=2.
        // Le frecce tastiera sono %K_LEFT/%K_RIGHT, quindi LEFT/CENTER/RIGHT sono liberi.
        AddConst("LEFT", 0);
        AddConst("CENTER", 1);
        AddConst("RIGHT", 2);

        // ex-COLOR/BUTTON/BORDER
        AddConst("BLACK", 0);
        AddConst("MAROON", 1);
        AddConst("GREEN", 2);
        AddConst("OLIVE", 3);
        AddConst("NAVY", 4);
        AddConst("PURPLE", 5);
        AddConst("TEAL", 6);
        AddConst("SILVER", 7);
        AddConst("GRAY", 8);
        AddConst("RED", 9);
        AddConst("LIME", 10);
        AddConst("YELLOW", 11);
        AddConst("BLUE", 12);
        AddConst("FUCHSIA", 13);
        AddConst("AQUA", 14);
        AddConst("WHITE", 15);
        AddConst("GRAY0", 16);
        AddConst("NAVY_BLUE", 17);
        AddConst("DARK_BLUE", 18);
        AddConst("BLUE3", 19);
        AddConst("BLUE3_2", 20);
        AddConst("BLUE1", 21);
        AddConst("DARK_GREEN", 22);
        AddConst("DEEP_SKY_BLUE4", 23);
        AddConst("DEEP_SKY_BLUE4_2", 24);
        AddConst("DEEP_SKY_BLUE4_3", 25);
        AddConst("DODGER_BLUE3", 26);
        AddConst("DODGER_BLUE2", 27);
        AddConst("GREEN4", 28);
        AddConst("SPRING_GREEN4", 29);
        AddConst("TURQUOISE4", 30);
        AddConst("DEEP_SKY_BLUE3", 31);
        AddConst("DEEP_SKY_BLUE3_2", 32);
        AddConst("DODGER_BLUE1", 33);
        AddConst("GREEN3", 34);
        AddConst("SPRING_GREEN3", 35);
        AddConst("DARK_CYAN", 36);
        AddConst("LIGHT_SEA_GREEN", 37);
        AddConst("DEEP_SKY_BLUE2", 38);
        AddConst("DEEP_SKY_BLUE1", 39);
        AddConst("GREEN3_2", 40);
        AddConst("SPRING_GREEN3_2", 41);
        AddConst("SPRING_GREEN2", 42);
        AddConst("CYAN3", 43);
        AddConst("DARK_TURQUOISE", 44);
        AddConst("TURQUOISE2", 45);
        AddConst("GREEN1", 46);
        AddConst("SPRING_GREEN2_2", 47);
        AddConst("SPRING_GREEN1", 48);
        AddConst("MEDIUM_SPRING_GREEN", 49);
        AddConst("CYAN2", 50);
        AddConst("CYAN1", 51);
        AddConst("DARK_RED", 52);
        AddConst("DEEP_PINK4", 53);
        AddConst("PURPLE4", 54);
        AddConst("PURPLE4_2", 55);
        AddConst("PURPLE3", 56);
        AddConst("BLUE_VIOLET", 57);
        AddConst("ORANGE4", 58);
        AddConst("GRAY37", 59);
        AddConst("MEDIUM_PURPLE4", 60);
        AddConst("SLATE_BLUE3", 61);
        AddConst("SLATE_BLUE3_2", 62);
        AddConst("ROYAL_BLUE1", 63);
        AddConst("CHARTREUSE4", 64);
        AddConst("DARK_SEA_GREEN4", 65);
        AddConst("PALE_TURQUOISE4", 66);
        AddConst("STEEL_BLUE", 67);
        AddConst("STEEL_BLUE3", 68);
        AddConst("CORNFLOWER_BLUE", 69);
        AddConst("CHARTREUSE3", 70);
        AddConst("DARK_SEA_GREEN4_2", 71);
        AddConst("CADET_BLUE", 72);
        AddConst("CADET_BLUE_2", 73);
        AddConst("SKY_BLUE3", 74);
        AddConst("STEEL_BLUE1", 75);
        AddConst("CHARTREUSE3_2", 76);
        AddConst("PALE_GREEN3", 77);
        AddConst("SEA_GREEN3", 78);
        AddConst("AQUAMARINE3", 79);
        AddConst("MEDIUM_TURQUOISE", 80);
        AddConst("STEEL_BLUE1_2", 81);
        AddConst("CHARTREUSE2", 82);
        AddConst("SEA_GREEN2", 83);
        AddConst("SEA_GREEN1", 84);
        AddConst("SEA_GREEN1_2", 85);
        AddConst("AQUAMARINE1", 86);
        AddConst("DARK_SLATE_GRAY2", 87);
        AddConst("DARK_RED_2", 88);
        AddConst("DEEP_PINK4_2", 89);
        AddConst("DARK_MAGENTA", 90);
        AddConst("DARK_MAGENTA_2", 91);
        AddConst("DARK_VIOLET", 92);
        AddConst("PURPLE1", 93);
        AddConst("ORANGE4_2", 94);
        AddConst("LIGHT_PINK4", 95);
        AddConst("PLUM4", 96);
        AddConst("MEDIUM_PURPLE3", 97);
        AddConst("MEDIUM_PURPLE3_2", 98);
        AddConst("SLATE_BLUE1", 99);
        AddConst("YELLOW4", 100);
        AddConst("WHEAT4", 101);
        AddConst("GRAY53", 102);
        AddConst("LIGHT_SLATE_GRAY", 103);
        AddConst("MEDIUM_PURPLE", 104);
        AddConst("LIGHT_SLATE_BLUE", 105);
        AddConst("YELLOW4_2", 106);
        AddConst("DARK_OLIVE_GREEN3", 107);
        AddConst("DARK_SEA_GREEN", 108);
        AddConst("LIGHT_SKY_BLUE3", 109);
        AddConst("LIGHT_SKY_BLUE3_2", 110);
        AddConst("SKY_BLUE2", 111);
        AddConst("CHARTREUSE2_2", 112);
        AddConst("DARK_OLIVE_GREEN3_2", 113);
        AddConst("PALE_GREEN3_2", 114);
        AddConst("DARK_SEA_GREEN3", 115);
        AddConst("DARK_SLATE_GRAY3", 116);
        AddConst("SKY_BLUE1", 117);
        AddConst("CHARTREUSE1", 118);
        AddConst("LIGHT_GREEN", 119);
        AddConst("LIGHT_GREEN_2", 120);
        AddConst("PALE_GREEN1", 121);
        AddConst("AQUAMARINE1_2", 122);
        AddConst("DARK_SLATE_GRAY1", 123);
        AddConst("GRAY2", 232);
        AddConst("GRAY3", 233);
        AddConst("GRAY7", 234);
        AddConst("GRAY11", 235);
        AddConst("GRAY15", 236);
        AddConst("GRAY19", 237);
        AddConst("GRAY23", 238);
        AddConst("GRAY27", 239);
        AddConst("GRAY30", 240);
        AddConst("GRAY35", 241);
        AddConst("GRAY39", 242);
        AddConst("GRAY42", 243);
        AddConst("GRAY46", 244);
        AddConst("GRAY50", 245);
        AddConst("GRAY54", 246);
        AddConst("GRAY58", 247);
        AddConst("GRAY62", 248);
        AddConst("GRAY66", 249);
        AddConst("GRAY70", 250);
        AddConst("GRAY74", 251);
        AddConst("GRAY78", 252);
        AddConst("GRAY82", 253);
        AddConst("GRAY85", 254);
        AddConst("GRAY89", 255);
        AddConst("OK", 1);
        AddConst("CANCEL", 2);
        AddConst("ABORT", 3);
        AddConst("RETRY", 4);
        AddConst("IGNORE", 5);
        AddConst("YES", 6);
        AddConst("NO", 7);
        AddConst("OKCANCEL", 101);
        AddConst("YESNO", 102);
        AddConst("YESNOCANCEL", 103);
        AddConst("ABORTRETRYIGNORE", 104);
        AddConst("RETRYCANCEL", 105);
        AddConst("CLOSED", -1);
        AddConst("NONE", 0);
        AddConst("SINGLE", 1);
        AddConst("DOUBLE", 2);
        AddConst("ROUNDED", 3);

        // ex-CONSOLE: modalita' console (CONSOLE n) — %SDL %C64 %ANSI %SYSTEM
        AddConst("SDL", 0);
        AddConst("C64", 1);
        AddConst("ANSI", 2);
        AddConst("SYSTEM", 3);

        // ex-JOY
        AddConst("JOYUP", 1);
        AddConst("JOYDOWN", 2);
        AddConst("JOYLEFT", 4);
        AddConst("JOYRIGHT", 8);
        AddConst("JOYFIRE", 16);
        AddConst("JOYBUTTON2", 32);

        // ex-IO (FILE_*)
        AddConst("FILE_CREATED", 1);
        AddConst("FILE_DELETED", 2);
        AddConst("FILE_CHANGED", 4);
        AddConst("FILE_RENAMED", 8);

        // ex-KEY.* (valori invariati rispetto al vecchio Compiler/Compiler.Constants.cs::KeyConstants)
        // Tasti Funzione
        AddConst("K_F1", (double)ConsoleKey.F1); AddConst("K_F2", (double)ConsoleKey.F2);
        AddConst("K_F3", (double)ConsoleKey.F3); AddConst("K_F4", (double)ConsoleKey.F4);
        AddConst("K_F5", (double)ConsoleKey.F5); AddConst("K_F6", (double)ConsoleKey.F6);
        AddConst("K_F7", (double)ConsoleKey.F7); AddConst("K_F8", (double)ConsoleKey.F8);
        AddConst("K_F9", (double)ConsoleKey.F9); AddConst("K_F10", (double)ConsoleKey.F10);
        AddConst("K_F11", (double)ConsoleKey.F11); AddConst("K_F12", (double)ConsoleKey.F12);

        // Tasti di Navigazione
        AddConst("K_UP", (double)ConsoleKey.UpArrow); AddConst("K_DOWN", (double)ConsoleKey.DownArrow);
        AddConst("K_LEFT", (double)ConsoleKey.LeftArrow); AddConst("K_RIGHT", (double)ConsoleKey.RightArrow);
        AddConst("K_HOME", (double)ConsoleKey.Home); AddConst("K_END", (double)ConsoleKey.End);
        AddConst("K_PGUP", (double)ConsoleKey.PageUp); AddConst("K_PGDN", (double)ConsoleKey.PageDown);

        // Altri Tasti
        AddConst("K_INS", (double)ConsoleKey.Insert); AddConst("K_DEL", (double)ConsoleKey.Delete);
        AddConst("K_ESC", (double)ConsoleKey.Escape); AddConst("K_TAB", (double)ConsoleKey.Tab);
        AddConst("K_ENTER", (double)ConsoleKey.Enter);
        AddConst("K_BS", (double)ConsoleKey.Backspace); AddConst("K_SPC", (double)ConsoleKey.Spacebar);
        AddConst("K_PAUSE", (double)ConsoleKey.Pause);

        // Capability AI: %CHAT=%OCR=%AUDIO + ruoli registry non-LLM %EMBEDDING/%RERANKER
        // (vedi AI-Provider-Register-Unified-Plan). Costanti GLOBALI pubbliche: l'AI namespace
        // e i comandi AI REGISTER/AI SET ACTIVE le usano direttamente (mai stringhe sostitutive).
        AddConst("CHAT", 0); AddConst("OCR", 1); AddConst("AUDIO", 2);
        AddConst("EMBEDDING", 3); AddConst("RERANKER", 4);
        // F4 multimodale (2026-09-11): capability TTS (sintesi vocale).
        AddConst("TTS", 5);

        // Modo reranker del retrieval RAG (P1-2b Second-Brain-Retrieval-Upgrades): valore del
        // parametro `reranker` nelle funzioni di ricerca (AI.SEARCHMULTI/AIVECTOR.SEARCH ecc.).
        // %RERANKER (ruolo registry, sopra) e' distinto: qui e' la SELEZIONE del reranker
        // usato nella ricerca. %RERANK_OFF disattiva anche il default; %RERANK_KEYWORD (default)
        // = KeywordReranker zero-dep; %RERANK_ONNX riservato al cross-encoder (non ancora wired).
        AddConst("RERANK_OFF", 0); AddConst("RERANK_KEYWORD", 1); AddConst("RERANK_ONNX", 2);

        // Motore browser (CUA/Universal Action Runtime): valore del parametro engine/provider
        // di BROWSER.START e della chiave agente BROWSER — mai stringhe "auto"/"chromium"/"obscura"
        // nel BASIC (controllo in fase di compilazione). %AUTO=0 (default) lascia la scelta
        // all'host; %CHROMIUM=1 Chromium headless (CDP); %OBSCURA=2 Obscura.
        AddConst("AUTO", 0); AddConst("CHROMIUM", 1); AddConst("OBSCURA", 2);

        // Flag ricezione canale (parità Go v, ok := <-ch): %RECEIVED=1 quando RECEIVE ha
        // letto un valore reale, %CHCLOSED=0 quando il canale è chiuso e svuotato. NOTA:
        // %CLOSED (=-1) esiste già per lo stile finestra (WINDOW FRAME CLOSED) — qui si usa
        // un nome dedicato per evitare collisioni. Usati con `RECEIVE v FROM ch OK ok`.
        AddConst("RECEIVED", 1); AddConst("CHCLOSED", 0);
        AddConst("K_PRTSC", (double)ConsoleKey.PrintScreen);

        // Tasti LETTERA A-Z
        AddConst("K_A", (double)ConsoleKey.A); AddConst("K_B", (double)ConsoleKey.B);
        AddConst("K_C", (double)ConsoleKey.C); AddConst("K_D", (double)ConsoleKey.D);
        AddConst("K_E", (double)ConsoleKey.E); AddConst("K_F", (double)ConsoleKey.F);
        AddConst("K_G", (double)ConsoleKey.G); AddConst("K_H", (double)ConsoleKey.H);
        AddConst("K_I", (double)ConsoleKey.I); AddConst("K_J", (double)ConsoleKey.J);
        AddConst("K_K", (double)ConsoleKey.K); AddConst("K_L", (double)ConsoleKey.L);
        AddConst("K_M", (double)ConsoleKey.M); AddConst("K_N", (double)ConsoleKey.N);
        AddConst("K_O", (double)ConsoleKey.O); AddConst("K_P", (double)ConsoleKey.P);
        AddConst("K_Q", (double)ConsoleKey.Q); AddConst("K_R", (double)ConsoleKey.R);
        AddConst("K_S", (double)ConsoleKey.S); AddConst("K_T", (double)ConsoleKey.T);
        AddConst("K_U", (double)ConsoleKey.U); AddConst("K_V", (double)ConsoleKey.V);
        AddConst("K_W", (double)ConsoleKey.W); AddConst("K_X", (double)ConsoleKey.X);
        AddConst("K_Y", (double)ConsoleKey.Y); AddConst("K_Z", (double)ConsoleKey.Z);

        // Tasti NUMERICI D0-D9
        AddConst("K_0", (double)ConsoleKey.D0); AddConst("K_1", (double)ConsoleKey.D1);
        AddConst("K_2", (double)ConsoleKey.D2); AddConst("K_3", (double)ConsoleKey.D3);
        AddConst("K_4", (double)ConsoleKey.D4); AddConst("K_5", (double)ConsoleKey.D5);
        AddConst("K_6", (double)ConsoleKey.D6); AddConst("K_7", (double)ConsoleKey.D7);
        AddConst("K_8", (double)ConsoleKey.D8); AddConst("K_9", (double)ConsoleKey.D9);

        // Tasti Numpad
        AddConst("K_N0", (double)ConsoleKey.NumPad0); AddConst("K_N1", (double)ConsoleKey.NumPad1);
        AddConst("K_N2", (double)ConsoleKey.NumPad2); AddConst("K_N3", (double)ConsoleKey.NumPad3);
        AddConst("K_N4", (double)ConsoleKey.NumPad4); AddConst("K_N5", (double)ConsoleKey.NumPad5);
        AddConst("K_N6", (double)ConsoleKey.NumPad6); AddConst("K_N7", (double)ConsoleKey.NumPad7);
        AddConst("K_N8", (double)ConsoleKey.NumPad8); AddConst("K_N9", (double)ConsoleKey.NumPad9);

        // Tasti Speciali
        AddConst("K_NDIV", (double)ConsoleKey.Divide); AddConst("K_NMUL", (double)ConsoleKey.Multiply);
        AddConst("K_NSUB", (double)ConsoleKey.Subtract); AddConst("K_NADD", (double)ConsoleKey.Add);
        AddConst("K_NDEC", (double)ConsoleKey.Decimal);

        // Combinazioni (costruite da KEY.MAKE a runtime, placeholder per compilazione)
        AddConst("K_CTRL_C", -1.0);
        AddConst("K_CTRL_BRK", -1.0);
        AddConst("K_SHIFT_TAB", -1.0);

        // Costanti per i Modificatori (usate in KEY.MAKE)
        AddConst("K_ALT", (double)ConsoleModifiers.Alt);
        AddConst("K_SHIFT", (double)ConsoleModifiers.Shift);
        AddConst("K_CTRL", (double)ConsoleModifiers.Control);

        // G4: governance profile costanti
        AddConst("FAST", 1.0);
        AddConst("GOVERNED", 2.0);

        // Capability DECIDE (JEV/SystemOne, append-only: valore 6). Accodata in fondo di
        // proposito: gli id canonici delle costanti sono posizionali e questa posizione non
        // sposta nessuna costante esistente.
        AddConst("DECIDE", 6);

        RegisterBuiltinNamespace(namespaceName, [], ImmutableDictionary<string, DataType>.Empty, constants: constants);
    }
}
