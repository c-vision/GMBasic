' EXPECT: thing_body= 16711680|thing_edge= 8943360|thing_below= 988969
' scene_thing — `SCENE THING n, x, y, z, w, h` (M-THINGS): billboard con la texture corrente.
'
' Thing 0 con base in (0,0,0), largo 2 e alto 2: occupa la metà alta del frame
' (y = altezza, la convenzione della superficie SCENE). La picture è
' `scene_alpha.png` (8x8): bordo 2px trasparente, centro 4x4 rosso opaco, e il
' thing è disegnato `masked` (alpha 0 = trasparente, C-INDEX0).
'
'   1. thing_body: (8,2) cade nel centro opaco → 0xFF0000 = 16711680;
'   2. thing_edge: (8,0) è il bordo superiore del billboard, dove il campione
'      bilineare mescola il rosso opaco e il verde trasparente → 0x887700
'      = 8943360 (l'alpha-test scarta solo dove l'alpha interpolato è 0);
'   3. thing_below: (8,6) è sotto la base del billboard → fondo 988969.
IMPORT SCENE

SUB Main()
  SCENE VIEW 16, 16, 60
  SCENE TEXTURE "scene_alpha.png"
  SCENE THING 0, 0, 0, 0, 2, 2
  SCENE CAMERA 0, 0, -3, 0, 0
  SCENE RENDER
  PRINT "thing_body="; SCENE.PIXEL(8, 2)
  PRINT "thing_edge="; SCENE.PIXEL(8, 0)
  PRINT "thing_below="; SCENE.PIXEL(8, 6)
END SUB
