' FILE: include/WasmBridge.inc
' =================================================================================================
'   WASMBRIDGE — helper per target WASM (.bas → .wasm, Web-Vision-Ext §2)
' =================================================================================================
' Libreria di sistema PURA (BASIC, cross-backend). Fornisce le DECLARE di import JS per il
' target WASM e costanti editabili per la shell HTML. Il modulo WASM (compilato da emitter C#
' → clang --target=wasm32) importa queste funzioni da `env` e le chiama; il JS glue
' (webapp/static/wasm.js) le fornisce.
'
' Uso:
'   IMPORT WASMBRIDGE
'
'   SUB Main()
'       WASMBRIDGE.Clear(0)
'       WASMBRIDGE.PSet(10, 10, 4)
'       WASMBRIDGE.Line(0, 0, 100, 100, 7)
'       WASMBRIDGE.JsAlert("Hello from WASM")
'   END SUB
'
' Compilazione (POC, zero C#):
'   dotnet run --project REPL/REPL.csproj -- -c99 prog.bas -o /tmp/prog.c
'   clang --target=wasm32 -o webapp/static/prog.wasm /tmp/prog.c native/gmb_runtime_c/src/*.c
' =================================================================================================

NAMESPACE WASMBRIDGE

' --- Costanti editabili (shell) ---

PUBLIC CONST JS_ENV$ = "env"
PUBLIC CONST CANVAS_ID$ = "gmb-canvas"
PUBLIC CONST WASM_PATH$ = "/static/prog.wasm"
PUBLIC CONST PROTOCOL_VERSION% = 1

' --- Import JS: canvas (chiamano wasm.js → <canvas>) ---

' DECLARE SUB WasmPSet LIB "env" ALIAS "wasm_pset" (x AS LONG, y AS LONG, c AS LONG)
' DECLARE SUB WasmLine LIB "env" ALIAS "wasm_line" (x1 AS LONG, y1 AS LONG, x2 AS LONG, y2 AS LONG, c AS LONG)
' DECLARE SUB WasmRect LIB "env" ALIAS "wasm_rect" (x AS LONG, y AS LONG, w AS LONG, h AS LONG, c AS LONG)
' DECLARE SUB WasmCircle LIB "env" ALIAS "wasm_circle" (x AS LONG, y AS LONG, r AS LONG, c AS LONG)
' DECLARE SUB WasmClear LIB "env" ALIAS "wasm_clear" (c AS LONG)
' DECLARE SUB WasmText LIB "env" ALIAS "wasm_text" (x AS LONG, y AS LONG, txt AS STRING, c AS LONG)

' --- Import JS: utility ---

' DECLARE SUB JsAlert LIB "env" ALIAS "alert" (msg AS STRING)
' DECLARE FUNCTION JsPrompt LIB "env" ALIAS "prompt" (msg AS STRING) AS STRING
' DECLARE SUB JsLog LIB "env" ALIAS "console_log" (msg AS STRING)
' DECLARE FUNCTION JsNow LIB "env" ALIAS "now_ms" () AS LONG

' --- Helper BASIC che wrappano gli import (editabili) ---

PUBLIC SUB Clear(c AS LONG)
    ' stub: su IL/VM nativo no-op; su wasm32 chiama WasmClear
    ' WasmClear(c)
END SUB

PUBLIC SUB PSet(x AS LONG, y AS LONG, c AS LONG)
    ' stub
    ' WasmPSet(x, y, c)
END SUB

PUBLIC SUB Line(x1 AS LONG, y1 AS LONG, x2 AS LONG, y2 AS LONG, c AS LONG)
    ' stub
    ' WasmLine(x1, y1, x2, y2, c)
END SUB

' --- Info ---

PUBLIC FUNCTION Info() AS STRING
    RETURN "WasmBridge v1"
END FUNCTION
