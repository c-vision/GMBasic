' EXPECT: v= 28|t= 14|d= 11|c= 6|h= 0|h2= 1|px=-1
' MOTORE 3D — dominio SCENE (renderer mesh, P3): geometria accumulata, contatori del pass,
' determinismo del frame e lettura PIXEL fuori dai limiti.
'
' La scena è volutamente piccola e fissa: un piano (4 vertici, 2 triangoli) e un cubo
' (24 vertici, 12 triangoli) = 28 vertici e 14 triangoli. DRAWN/CULLED dipendono dal frustum
' del kernel, non dal linguaggio: se cambiano, è cambiato il renderer — ed è esattamente
' quello che il test deve far vedere. `h` è (hash del secondo RENDER − hash del primo): 0
' significa che due frame identici producono lo stesso contenuto (nessuno stato che si
' accumula). `px` = SCENE.PIXEL(-1, 0) = −1: fuori target è una sentinella, non un colore.
IMPORT SCENE

SUB Main()
  SCENE RESET
  SCENE VIEW 64, 32, 60
  SCENE TEXTURE 4

  ' Piano 20x20 sul terreno, antiorario visto da sopra (frontale per il kernel).
  SCENE OBJECT 90, 150, 220
  SCENE VERTEX -10, 0, -10, 0, 0
  SCENE VERTEX -10, 0,  10, 0, 4
  SCENE VERTEX  10, 0,  10, 4, 4
  SCENE VERTEX  10, 0, -10, 4, 0
  SCENE QUAD 0, 1, 2, 3

  ' Cubo 2x2x2 appoggiato sul piano, con la sua posizione.
  SCENE OBJECT 220, 120, 60
  SCENE BOX -1, 0, -1, 1, 2, 1
  SCENE OBJPOS 0, 0, 0, 0

  SCENE CAMERA 8, 5, 10, 218, -14
  SCENE RENDER

  PRINT "v="; SCENE.VERTS()
  PRINT "t="; SCENE.TRIS()
  PRINT "d="; SCENE.DRAWN()
  PRINT "c="; SCENE.CULLED()

  primo = SCENE.HASH()
  SCENE RENDER
  PRINT "h="; SCENE.HASH() - primo
  IF SCENE.HASH() - primo = 0 THEN
    PRINT "h2= 1"
  ELSE
    PRINT "h2= 0"
  END IF

  PRINT "px="; SCENE.PIXEL(-1, 0)
END SUB
