// FILE: webapp/static/canvas.js — Canvas Remoto GMBasic (editabile)
// Replay dei draw commands serializzati da include/WebCanvas.inc (WEBCANVAS.PSet/Line/Rect/…)
// su un <canvas> HTML5. Protocollo V1 JSON: {op:"pset",x:10,y:10,c:4,v:1} (palette 0-15).

// Palette CGA 16 colori (0-15) → #rrggbb — editabile: cambia qui i colori del tema
const GMB_PALETTE = [
  "#000000","#0000AA","#00AA00","#00AAAA","#AA0000","#AA00AA","#AA5500","#AAAAAA",
  "#555555","#5555FF","#55FF55","#55FFFF","#FF5555","#FF55FF","#FFFF55","#FFFFFF"
];
function palette(c){
  const idx = ((c|0) % 16 + 16) % 16;
  return GMB_PALETTE[idx];
}

// Disegna un singolo comando sul canvas (ctx già ottenuto)
function CanvasReplay(cmd, canvas){
  if(!cmd || !canvas) return;
  // compat: vecchie WS inviano {op:"pset"} senza v
  if(cmd.v && cmd.v !== 1){
    console.warn("[canvas] protocol version mismatch", cmd.v);
  }
  const ctx = canvas.getContext('2d');
  if(!ctx) return;
  switch((cmd.op||"").toLowerCase()){
    case "clear": {
      ctx.fillStyle = palette(cmd.c);
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      break;
    }
    case "pset": {
      ctx.fillStyle = palette(cmd.c);
      ctx.fillRect(cmd.x|0, cmd.y|0, 1, 1);
      break;
    }
    case "line": {
      ctx.strokeStyle = palette(cmd.c);
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(cmd.x1|0, cmd.y1|0);
      ctx.lineTo(cmd.x2|0, cmd.y2|0);
      ctx.stroke();
      break;
    }
    case "rect": {
      ctx.strokeStyle = palette(cmd.c);
      ctx.lineWidth = 1;
      ctx.strokeRect(cmd.x|0, cmd.y|0, cmd.w|0, cmd.h|0);
      break;
    }
    case "circle": {
      ctx.strokeStyle = palette(cmd.c);
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.arc(cmd.x|0, cmd.y|0, cmd.r|0, 0, Math.PI*2);
      ctx.stroke();
      break;
    }
    case "paint": {
      // flood fill semplice via ImageData BFS (fc = fill, bc = border opzionale)
      try{
        const fc = palette(cmd.fc);
        const w = canvas.width, h = canvas.height;
        const x0 = cmd.x|0, y0 = cmd.y|0;
        if(x0<0||x0>=w||y0<0||y0>=h) break;
        const img = ctx.getImageData(0,0,w,h);
        const data = img.data;
        const hexToRgb = (hex)=>{ const n=parseInt(hex.slice(1),16); return [(n>>16)&255,(n>>8)&255,n&255]; };
        const [fr,fg,fb] = hexToRgb(fc);
        const idx0 = (y0*w + x0)*4;
        const tr=data[idx0], tg=data[idx0+1], tb=data[idx0+2];
        if(tr===fr && tg===fg && tb===fb) break;
        const stack=[[x0,y0]];
        const seen=new Uint8Array(w*h);
        const at=(x,y)=> (y*w+x);
        while(stack.length){
          const [x,y]=stack.pop();
          if(x<0||x>=w||y<0||y>=h) continue;
          const p=at(x,y); if(seen[p]) continue; seen[p]=1;
          const i=p*4; if(data[i]!==tr||data[i+1]!==tg||data[i+2]!==tb) continue;
          data[i]=fr; data[i+1]=fg; data[i+2]=fb; data[i+3]=255;
          stack.push([x+1,y],[x-1,y],[x,y+1],[x,y-1]);
        }
        ctx.putImageData(img,0,0);
      }catch(e){ console.warn("[canvas] paint error",e); }
      break;
    }
    case "putimage": {
      // data: [w*h] palette indices 0-15, mode "PSET"/"AND"/"OR"/"XOR" (per ora solo PSET)
      try{
        const w=cmd.w|0, h=cmd.h|0, x=cmd.x|0, y=cmd.y|0;
        const data=cmd.data; if(!Array.isArray(data) || w<=0 || h<=0) break;
        const imgData=ctx.createImageData(w,h);
        const mode=(cmd.mode||"PSET").toUpperCase();
        for(let i=0;i<w*h;i++){
          let ci=data[i]|0;
          const o=i*4;
          if(ci<0){ imgData.data[o+3]=0; continue; } // trasparente
          const hex=palette(ci);
          const n=parseInt(hex.slice(1),16);
          let r=(n>>16)&255, g=(n>>8)&255, b=n&255;
          if(mode!=="PSET"){
            // per AND/OR/XOR leggi il pixel esistente (se dentro canvas) e applica l'operatore
            // su indice palette, poi ricalcola rgb dal risultato
            try{
              const cx=x+(i%w), cy=y+Math.floor(i/w);
              if(cx>=0&&cx<canvas.width&&cy>=0&&cy<canvas.height){
                const cur=ctx.getImageData(cx,cy,1,1).data;
                // trova indice palette più vicino al colore corrente (inverso di palette)
                let curIdx=0, best=1e9;
                for(let k=0;k<16;k++){
                  const hk=palette(k), nk=parseInt(hk.slice(1),16);
                  const kr=(nk>>16)&255, kg=(nk>>8)&255, kb=nk&255;
                  const d=(kr-cur[0])**2+(kg-cur[1])**2+(kb-cur[2])**2;
                  if(d<best){best=d;curIdx=k;}
                }
                if(mode==="AND") ci=curIdx & ci;
                else if(mode==="OR") ci=curIdx | ci;
                else if(mode==="XOR"||mode==="NOT") ci=curIdx ^ ci;
                const hex2=palette(ci);
                const n2=parseInt(hex2.slice(1),16);
                r=(n2>>16)&255; g=(n2>>8)&255; b=n2&255;
              }
            }catch(e){}
          }
          imgData.data[o]=r; imgData.data[o+1]=g; imgData.data[o+2]=b; imgData.data[o+3]=255;
        }
        ctx.putImageData(imgData,x,y);
      }catch(e){ console.warn("[canvas] putimage error",e); }
      break;
    }
    case "getimage": {
      // richiesta di cattura: per ora logga, il BASIC riceve già il buffer locale via ReadActivePageToBuffer
      // Futuro: invia i pixel catturati al server via WS per round-trip
      try{
        const w=cmd.w|0, h=cmd.h|0, x=cmd.x|0, y=cmd.y|0;
        const d=ctx.getImageData(x,y,w,h);
        console.log("[canvas] getimage",x,y,w,h,"captured",d.data.length,"bytes");
      }catch(e){ console.warn("[canvas] getimage error",e); }
      break;
    }
    case "sprite": {
      // placeholder: rettangolo con id — l'utente sostituisce con drawImage reale
      ctx.fillStyle = "#e0e7ff";
      ctx.fillRect(cmd.x|0, cmd.y|0, 16, 16);
      ctx.fillStyle = "#3730a3";
      ctx.font = "10px monospace";
      ctx.fillText(String(cmd.id), (cmd.x|0)+3, (cmd.y|0)+11);
      break;
    }
    case "text": {
      ctx.fillStyle = palette(cmd.c);
      ctx.font = "13px -apple-system, sans-serif";
      ctx.fillText(String(cmd.txt||""), cmd.x|0, cmd.y|0);
      break;
    }
    case "flush": {
      // fine frame — no-op, l'utente può aggiungere requestAnimationFrame batching qui
      break;
    }
    default:
      console.warn("[canvas] unknown op", cmd);
  }
}

// Batch: array di comandi → replay sequenziale
function CanvasReplayBatch(cmds, canvas){
  if(!Array.isArray(cmds)) return CanvasReplay(cmds, canvas);
  for(const c of cmds) CanvasReplay(c, canvas);
}

// Export per wasm.js / index.bsp (global)
if(typeof window !== "undefined"){
  window.CanvasReplay = CanvasReplay;
  window.CanvasReplayBatch = CanvasReplayBatch;
  window.GMB_PALETTE = GMB_PALETTE;
}
