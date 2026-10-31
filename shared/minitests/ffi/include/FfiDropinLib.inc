NAMESPACE FFIDROPIN

' Drop-in FFI (auto-discovery `libs/`): la dylib NON sta accanto al programma ma in
' `minitests/ffi/libs/libffidropin.dylib`. Il nome dichiarato `libffidropin` non esiste
' in CWD → la risoluzione DEVE passare dal nuovo walk-up `libs/` (FfiService/gmb_ffi_dl).
PUBLIC DECLARE FUNCTION dropin_scale LIB "libffidropin" ALIAS "vec_scale" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION dropin_add LIB "libffidropin" ALIAS "vec_add" (a AS LONG, b AS LONG) AS LONG
PUBLIC DECLARE FUNCTION dropin_prime LIB "libffidropin" ALIAS "vec_isprime" (n AS LONG) AS LONG
