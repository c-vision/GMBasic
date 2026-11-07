' xform.inc — namespace XFORM: trasformazioni 3D (gmbmath-vector, FFI).
'
' Le matrici sono array PIATTI di DOUBLE in ordine ROW-MAJOR, come in GFX3D e in
' QUAT.TOMAT3: 9 valori per la 3x3, 16 per la 4x4 omogenea.
'
'   DIM m3(8) AS DOUBLE          ' 3x3: m3(riga*3 + colonna)
'   DIM m4(15) AS DOUBLE         ' 4x4: m4(riga*4 + colonna)
'
' Una 4x4 omogenea rappresenta rotazione + traslazione (+ scala); l'ultima colonna
' (m(3), m(7), m(11), m(15)) è la traslazione, l'ultima riga è (0,0,0,1) per le
' trasformazioni affini.
'
' Perché non gli handle di MATRIX: XFORM è la semantica della trasformazione (rotazione
' attorno a un asse, traslazione, punto contro direzione, inversa affine, matrice
' normale), non un contenitore di matrici. Determinante, inversa completa e trasposta
' restano in MATRIX/LINALG.
'
' MUL3/MUL4 compongono due buffer piatti: è l'operatore della rappresentazione (le due
' matrici sono già in mano al chiamante). MATRIX.MULTIPLY lavora su handle e va usato
' quando le matrici sono gestite da MATRIX.
'
' Ritorno: 0 = ok, -1 = argomento non valido o matrice singolare.

NAMESPACE XFORM

' ── 3x3: rotazioni e normali ────────────────────────────────────────────────
' Matrice identità 3x3.
DECLARE FUNCTION MAT3IDENTITY LIB "gmbmath" ALIAS "ofx_xf_mat3_identity" (m() AS DOUBLE) AS LONG
' Rotazione di `rad` radianti attorno a X, Y, Z.
DECLARE FUNCTION MAT3ROTX LIB "gmbmath" ALIAS "ofx_xf_mat3_rotx" (rad AS DOUBLE, m() AS DOUBLE) AS LONG
DECLARE FUNCTION MAT3ROTY LIB "gmbmath" ALIAS "ofx_xf_mat3_roty" (rad AS DOUBLE, m() AS DOUBLE) AS LONG
DECLARE FUNCTION MAT3ROTZ LIB "gmbmath" ALIAS "ofx_xf_mat3_rotz" (rad AS DOUBLE, m() AS DOUBLE) AS LONG
' Prodotto a*b (prima b, poi a) fra due 3x3.
DECLARE FUNCTION MAT3MUL LIB "gmbmath" ALIAS "ofx_xf_mat3_mul" (a() AS DOUBLE, b() AS DOUBLE, out_() AS DOUBLE) AS LONG
' Matrice normale (M^-1)^T per trasformare le normali. -1 se M è singolare.
DECLARE FUNCTION MAT3NORMAL LIB "gmbmath" ALIAS "ofx_xf_mat3_normal" (a() AS DOUBLE, out_() AS DOUBLE) AS LONG
' Estrae la sottomatrice 3x3 (rotazione) da una 4x4 omogenea.
DECLARE FUNCTION MAT3FROM4 LIB "gmbmath" ALIAS "ofx_xf_mat3_from4" (m4() AS DOUBLE, out_() AS DOUBLE) AS LONG

' ── 4x4: trasformazioni omogenee ────────────────────────────────────────────
DECLARE FUNCTION MAT4IDENTITY LIB "gmbmath" ALIAS "ofx_xf_mat4_identity" (m() AS DOUBLE) AS LONG
' Traslazione (x, y, z).
DECLARE FUNCTION MAT4TRANSLATE LIB "gmbmath" ALIAS "ofx_xf_mat4_translate" (x AS DOUBLE, y AS DOUBLE, z AS DOUBLE, m() AS DOUBLE) AS LONG
' Scala (x, y, z) sugli assi.
DECLARE FUNCTION MAT4SCALE LIB "gmbmath" ALIAS "ofx_xf_mat4_scale" (x AS DOUBLE, y AS DOUBLE, z AS DOUBLE, m() AS DOUBLE) AS LONG
' Rotazione di `rad` radianti attorno a X, Y, Z.
DECLARE FUNCTION MAT4ROTX LIB "gmbmath" ALIAS "ofx_xf_mat4_rotx" (rad AS DOUBLE, m() AS DOUBLE) AS LONG
DECLARE FUNCTION MAT4ROTY LIB "gmbmath" ALIAS "ofx_xf_mat4_roty" (rad AS DOUBLE, m() AS DOUBLE) AS LONG
DECLARE FUNCTION MAT4ROTZ LIB "gmbmath" ALIAS "ofx_xf_mat4_rotz" (rad AS DOUBLE, m() AS DOUBLE) AS LONG
' Prodotto a*b (prima b, poi a) fra due 4x4.
DECLARE FUNCTION MAT4MUL LIB "gmbmath" ALIAS "ofx_xf_mat4_mul" (a() AS DOUBLE, b() AS DOUBLE, out_() AS DOUBLE) AS LONG
' Inversa di una trasformazione affine (traslazione+rotazione+scala). -1 se singolare.
DECLARE FUNCTION MAT4INVERSEAFFINE LIB "gmbmath" ALIAS "ofx_xf_mat4_inverse_affine" (a() AS DOUBLE, out_() AS DOUBLE) AS LONG
' Trasforma il punto (x,y,z): out_() riceve 3 double (con divisione per w).
DECLARE FUNCTION MAT4POINT LIB "gmbmath" ALIAS "ofx_xf_mat4_point" (m() AS DOUBLE, x AS DOUBLE, y AS DOUBLE, z AS DOUBLE, out_() AS DOUBLE) AS LONG
' Trasforma la direzione (x,y,z): la traslazione non la tocca. out_() = 3 double.
DECLARE FUNCTION MAT4DIR LIB "gmbmath" ALIAS "ofx_xf_mat4_dir" (m() AS DOUBLE, x AS DOUBLE, y AS DOUBLE, z AS DOUBLE, out_() AS DOUBLE) AS LONG
' Costruisce la 4x4 omogenea da una 3x3 (traslazione nulla).
DECLARE FUNCTION MAT4FROM3 LIB "gmbmath" ALIAS "ofx_xf_mat4_from3" (m3() AS DOUBLE, out_() AS DOUBLE) AS LONG
