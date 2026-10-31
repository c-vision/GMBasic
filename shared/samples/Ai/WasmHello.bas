' FILE: AI/WasmHello.bas
' =================================================================
' Sample per WASM (Web-Vision-Ext §2) — WasmBridge.inc
' Mostra gli import JS dichiarati in include/WasmBridge.inc.
' Su target nativo (IL/VM/C) le chiamate WASM sono no-op / stub e il
' programma gira comunque (non crasha). Su target wasm32 (futuro:
' gmb prog.bas -target wasm) le stesse chiamate diventano import "env".
'
' Avvio nativo (oggi, zero toolchain):
'   dotnet run --project REPL/REPL.csproj -- samples/Community/AI/WasmHello.bas
' Avvio WASM (POC, quando clang wasm32 è disponibile):
'   dotnet run --project REPL/REPL.csproj -- -c99 samples/Community/AI/WasmHello.bas -o /tmp/hello.c
'   clang --target=wasm32 -o webapp/static/hello.wasm /tmp/hello.c native/gmb_runtime_c/src/*.c
'   poi apri http://localhost:8090/wasm.bsp
' =================================================================
IMPORT WASMBRIDGE

SUB Main()
    PRINT "=== WasmHello — WasmBridge.inc ==="
    PRINT WASMBRIDGE.Info()
    PRINT "Canvas: " + WASMBRIDGE.CANVAS_ID$
    PRINT

    ' Draw via WASM imports (no-op su IL/VM/C, draw su wasm32)
    WASMBRIDGE.Clear(0)
    WASMBRIDGE.PSet(10, 10, 4)
    WASMBRIDGE.Line(0, 0, 100, 100, 7)

    PRINT "WasmPSet/Line/Clear chiamati (no-op su nativo, draw su WASM)"
    PRINT "Shell HTML: webapp/pages/wasm.bsp + webapp/static/wasm.js"
    PRINT "Compila in WASM con: gmb prog.bas -target wasm -o webapp/static/prog.wasm (futuro)"
END SUB

