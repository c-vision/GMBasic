NAMESPACE FFINS

' DOUBLE (no ALIAS / ALIAS) — contratto all-double
PUBLIC DECLARE FUNCTION vec_scale LIB "libvec_module" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION ns_scale LIB "libvec_module" ALIAS "vec_scale" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
' LONG omogeneo (OP_CALL_FFI_T / wrapper int64)
PUBLIC DECLARE FUNCTION ns_add LIB "libvec_module" ALIAS "vec_add" (a AS LONG, b AS LONG) AS LONG
PUBLIC DECLARE FUNCTION ns_prime LIB "libvec_module" ALIAS "vec_isprime" (n AS LONG) AS LONG

