' EXPECT: tris= 24|objs= 2|draw= 4|cull= 20|px= 1|m1= 1|r2= 2|rmax= 2|case= 24|unk=-1|after= 0|tris0= 0|bytes= 1
' scene_stats — statistiche della scena, `SCENE.STATS(nome$)` (P5, specifica M-STATS).
'
' Le misure NON sono impressioni: sono gli stessi contatori che il banco e
' l'oracolo usano per dimostrare due invarianti della nota di thread/present —
' «RENDER non alloca per frame» (allocs) e «RENDER una volta per frame»
' (renders / renders_max). Il tempo di un frame non c'e': si misura fuori.
'
' Geometria (y = altezza, piano x/z, fronte a yaw 0 = +z): due box chiusi
' davanti alla camera, 12 triangoli ciascuno. Assiali e visti di fronte, di
' ogni box si disegnano solo le 2 facce che guardano la camera: le altre 10
' hanno area proiettata nulla e finiscono nei culled (2+10 per box).
' Gli stessi valori su C99Native, IL, PCode e VM/C.
IMPORT SCENE

SUB Main()
  SCENE RESET
  SCENE VIEW 32, 16, 60
  SCENE CAMERA 0, 46, 0, 0, 0

  SCENE OBJECT 200, 200, 200
  SCENE BOX -10, 20, 60, 10, 70, 70
  SCENE OBJECT 120, 200, 255
  SCENE BOX -10, 20, 90, 10, 70, 100

  ' ── 1. Geometria dichiarata, nessun RENDER ancora ────────────────────────
  PRINT "tris="; SCENE.STATS("tris")
  PRINT "objs="; SCENE.STATS("objs")

  ' ── 2. Un RENDER: contatori del pass, una sola chiamata nel frame ────────
  SCENE RENDER
  PRINT "draw="; SCENE.STATS("draw")
  PRINT "cull="; SCENE.STATS("culled")
  px = 0
  IF SCENE.STATS("pixels") > 0 THEN px = 1
  PRINT "px="; px
  PRINT "m1="; SCENE.STATS("renders")

  ' ── 3. Secondo RENDER nello stesso frame: renders cresce, max lo segue ───
  SCENE RENDER
  PRINT "r2="; SCENE.STATS("renders")
  PRINT "rmax="; SCENE.STATS("renders_max")

  ' ── 4. Le chiavi non distinguono maiuscole; una ignota e' -1 ─────────────
  PRINT "case="; SCENE.STATS("TRIS")
  PRINT "unk="; SCENE.STATS("nonesiste")

  ' ── 5. RESET azzera i contatori, NON la memoria riservata ────────────────
  SCENE RESET
  PRINT "after="; SCENE.STATS("renders")
  PRINT "tris0="; SCENE.STATS("tris")
  bytes = 0
  IF SCENE.STATS("bytes") > 0 THEN bytes = 1
  PRINT "bytes="; bytes
END SUB
