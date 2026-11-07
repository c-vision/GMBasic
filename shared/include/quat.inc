' quat.inc — namespace QUAT: quaternioni (gmbmath-vector, FFI).
'
' Un quaternione è il TYPE `Quat` (x, y, z vettoriali + w scalare), con lo stesso
' layout della struct C `gm_quat` (4 double). Viaggia **per valore**, come GeoPoint
' di GEO: `q = QUAT.MAKE(0, 0, 0, 1)`, nessun handle e nessuno stato.
'
' Convenzione: rotazioni con quaternioni NORMALIZZATI (usare NORMALIZE o gli esempi
' FROM*). Le matrici 3x3 sono 9 double in ordine ROW-MAJOR.
'
' Esiti: le funzioni che producono un valore lo restituiscono; solo INVERSE non ha
' risultato su un quaternione nullo e segnala l'errore in `st()`.

NAMESPACE QUAT

TYPE Quat
  X AS DOUBLE
  Y AS DOUBLE
  Z AS DOUBLE
  W AS DOUBLE
END TYPE

' ── costruzione ─────────────────────────────────────────────────────────────
' Quaternione identità (0,0,0,1).
DECLARE FUNCTION IDENTITY LIB "gmbmath" ALIAS "ofx_quat_identity" () AS Quat
' Quaternione esplicito (x,y,z,w); serve NORMALIZE per una rotazione valida.
DECLARE FUNCTION MAKE LIB "gmbmath" ALIAS "ofx_quat_make" (x AS DOUBLE, y AS DOUBLE, z AS DOUBLE, w AS DOUBLE) AS Quat
' Rotazione che porta +Z sulla direzione (x,y,z) (normalizzata internamente).
DECLARE FUNCTION FROMDIR LIB "gmbmath" ALIAS "ofx_quat_from_dir" (x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS Quat
' Rotazione di `radians` attorno all'asse (ax,ay,az).
DECLARE FUNCTION FROMAXISANGLE LIB "gmbmath" ALIAS "ofx_quat_from_axis_angle" (ax AS DOUBLE, ay AS DOUBLE, az AS DOUBLE, radians AS DOUBLE) AS Quat
' Rotazione che porta il vettore (fx,fy,fz) su (tx,ty,tz).
DECLARE FUNCTION FROMTWOVECTORS LIB "gmbmath" ALIAS "ofx_quat_from_two_vectors" (fx AS DOUBLE, fy AS DOUBLE, fz AS DOUBLE, tx AS DOUBLE, ty AS DOUBLE, tz AS DOUBLE) AS Quat
' Quaternione della matrice di rotazione 3x3 (m = 9 double row-major).
DECLARE FUNCTION FROMMAT3 LIB "gmbmath" ALIAS "ofx_quat_from_mat3" (m() AS DOUBLE) AS Quat

' ── algebra ─────────────────────────────────────────────────────────────────
DECLARE FUNCTION ADD LIB "gmbmath" ALIAS "ofx_quat_add" (a AS Quat, b AS Quat) AS Quat
DECLARE FUNCTION SCALE LIB "gmbmath" ALIAS "ofx_quat_scale" (a AS Quat, s AS DOUBLE) AS Quat
DECLARE FUNCTION NEG LIB "gmbmath" ALIAS "ofx_quat_neg" (a AS Quat) AS Quat
' Prodotto di Hamilton: la composizione di due rotazioni (prima a, poi b).
DECLARE FUNCTION MUL LIB "gmbmath" ALIAS "ofx_quat_mul" (a AS Quat, b AS Quat) AS Quat
DECLARE FUNCTION CONJUGATE LIB "gmbmath" ALIAS "ofx_quat_conjugate" (a AS Quat) AS Quat
DECLARE FUNCTION DOT LIB "gmbmath" ALIAS "ofx_quat_dot" (a AS Quat, b AS Quat) AS DOUBLE
DECLARE FUNCTION LENGTH LIB "gmbmath" ALIAS "ofx_quat_length" (a AS Quat) AS DOUBLE
' Porta la norma a 1; sul quaternione nullo restituisce l'identità.
DECLARE FUNCTION NORMALIZE LIB "gmbmath" ALIAS "ofx_quat_normalize" (a AS Quat) AS Quat
' Inversa: coniugato / |q|^2. st(0) = 0 se ok, -1 se il quaternione è nullo
' (in quel caso il valore restituito è zero).
DECLARE FUNCTION INVERSE LIB "gmbmath" ALIAS "ofx_quat_inverse" (a AS Quat, st() AS LONG) AS Quat

' ── interpolazione ──────────────────────────────────────────────────────────
' LERP normalizzato (risultato a norma 1), t in [0,1].
DECLARE FUNCTION LERP LIB "gmbmath" ALIAS "ofx_quat_lerp" (a AS Quat, b AS Quat, t AS DOUBLE) AS Quat
' SLERP: interpolazione sferica, t in [0,1] (sceglie l'emisfero corto).
DECLARE FUNCTION SLERP LIB "gmbmath" ALIAS "ofx_quat_slerp" (a AS Quat, b AS Quat, t AS DOUBLE) AS Quat

' ── conversione e applicazione ──────────────────────────────────────────────
' Matrice 3x3 row-major in m() (9 double). 0 = ok, -1 = array troppo corto.
DECLARE FUNCTION TOMAT3 LIB "gmbmath" ALIAS "ofx_quat_to_mat3" (a AS Quat, m() AS DOUBLE) AS LONG
' Ruota il vettore (x,y,z): out() riceve 3 double. 0 = ok, -1 = array troppo corto.
DECLARE FUNCTION ROTATEVEC3 LIB "gmbmath" ALIAS "ofx_quat_rotate_vec3" (a AS Quat, x AS DOUBLE, y AS DOUBLE, z AS DOUBLE, out_() AS DOUBLE) AS LONG
