' SpriteCollision.bas — COLLIDE Test
IMPORT GRAPHICS
'
' COLLIDE(id1%, id2%) → -1 (collision) or 0 (no collision)
' AABB collision detection via SDL2.

PRINT "=== COLLIDE Test ==="
PRINT ""

DIM id1% = 1
DIM id2% = 2
DIM id3% = 3

' Forma a 2 argomenti: AABB between two sprites
DIM c1% = COLLIDE(id1%, id2%)
IF c1% = -1 THEN
  PRINT "id1 vs id2: COLLISION"
ELSE
  PRINT "id1 vs id2: No collision (expected without SCREEN)"
END IF

DIM c2% = COLLIDE(id1%, id3%)
IF c2% = -1 THEN
  PRINT "id1 vs id3: COLLISION"
ELSE
  PRINT "id1 vs id3: No collision (expected without SCREEN)"
END IF

PRINT ""
PRINT "=== Fine ==="
