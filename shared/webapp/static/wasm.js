// FILE: webapp/static/wasm.js — Glue WASM GMBasic (editabile)
// Carica un modulo .wasm compilato da emitter C# (clang --target=wasm32) e collega gli import
// dichiarati in include/WasmBridge.inc (DECLARE LIB "env" ALIAS "wasm_pset" etc.) al <canvas>.

// Palette condivisa con canvas.js (duplicata per standalone)
const WASM_PALETTE = [
  "#000000","#0000AA","#00AA00","#00AAAA","#AA0000","#AA00AA","#AA5500","#AAAAAA",
  "#555555","#5555FF","#55FF55","#55FFFF","#FF5555","#FF55FF","#FFFF55","#FFFFFF"
];
function wasmPalette(c){
  const idx = ((c|0) % 16 + 16) % 16;
  return WASM_PALETTE[idx];
}

// Stato WASM corrente
let wasmInstance = null;
let wasmCanvas = null;

function getWasmCtx(){
  if(!wasmCanvas) return null;
  return wasmCanvas.getContext('2d');
}

// Import object — deve matchare le DECLARE in WasmBridge.inc
function makeWasmImports(canvas){
  wasmCanvas = canvas;
  return {
    env: {
      // canvas imports (chiamati dal C/WASM)
      wasm_pset: (x,y,c) => {
        const ctx = getWasmCtx(); if(!ctx) return;
        ctx.fillStyle = wasmPalette(c);
        ctx.fillRect(x|0, y|0, 1, 1);
      },
      wasm_line: (x1,y1,x2,y2,c) => {
        const ctx = getWasmCtx(); if(!ctx) return;
        ctx.strokeStyle = wasmPalette(c);
        ctx.lineWidth = 1;
        ctx.beginPath(); ctx.moveTo(x1|0,y1|0); ctx.lineTo(x2|0,y2|0); ctx.stroke();
      },
      wasm_rect: (x,y,w,h,c) => {
        const ctx = getWasmCtx(); if(!ctx) return;
        ctx.strokeStyle = wasmPalette(c);
        ctx.lineWidth = 1;
        ctx.strokeRect(x|0,y|0,w|0,h|0);
      },
      wasm_circle: (x,y,r,c) => {
        const ctx = getWasmCtx(); if(!ctx) return;
        ctx.strokeStyle = wasmPalette(c);
        ctx.lineWidth = 1;
        ctx.beginPath(); ctx.arc(x|0,y|0,r|0,0,Math.PI*2); ctx.stroke();
      },
      wasm_clear: (c) => {
        const ctx = getWasmCtx(); if(!ctx || !wasmCanvas) return;
        ctx.fillStyle = wasmPalette(c);
        ctx.fillRect(0,0,wasmCanvas.width,wasmCanvas.height);
      },
      wasm_text: (x,y,ptr,c) => {
        // ptr è un puntatore C a stringa — in POC senza memoria WASM condivisa,
        // questo stub logga; la versione Emscripten usa UTF8ToString(ptr)
        const ctx = getWasmCtx(); if(!ctx) return;
        ctx.fillStyle = wasmPalette(c);
        ctx.font = "13px monospace";
        ctx.fillText(String(ptr), x|0, y|0);
      },
      // utility imports
      alert: (msg) => alert(String(msg)),
      prompt: (msg) => prompt(String(msg)) || "",
      console_log: (msg) => console.log("[wasm]", String(msg)),
      now_ms: () => Date.now()|0,
      // Emscripten / WASI compat (se il modulo è linkato con emcc)
      __memory_base: 0,
      __table_base: 0,
    },
    wasi_snapshot_preview1: {
      // stub WASI per moduli che importano fd_write etc. — ritorna no-op
      fd_write: () => 0, proc_exit: () => {}, fd_close: () => 0, fd_seek: () => 0,
    }
  };
}

// Carica ed esegue un .wasm nel canvas dato
async function WasmLoad(wasmPath, canvas){
  const c = canvas || document.getElementById('wasm-canvas') || document.getElementById('gmb-canvas');
  if(!c) throw new Error("canvas not found (wasm-canvas or gmb-canvas)");
  const imports = makeWasmImports(c);
  const resp = await fetch(wasmPath);
  if(!resp.ok) throw new Error(`fetch ${wasmPath}: ${resp.status}`);
  const bytes = await resp.arrayBuffer();
  const { instance } = await WebAssembly.instantiate(bytes, imports);
  wasmInstance = instance;
  // chiama _start / main / Main se esportato
  const exp = instance.exports;
  if(typeof exp._start === "function") exp._start();
  else if(typeof exp.main === "function") exp.main();
  else if(typeof exp.Main === "function") exp.Main();
  else if(typeof exp._Main === "function") exp._Main();
  console.log("[wasm] loaded", wasmPath, "exports:", Object.keys(exp));
  return instance;
}

// Export global
if(typeof window !== "undefined"){
  window.WasmLoad = WasmLoad;
  window.makeWasmImports = makeWasmImports;
}
