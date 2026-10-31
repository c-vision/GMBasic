// ============================================================================
// math_go.go — modulo Go (cgo) per GMBasic, SOLO c-abi (nessun gmb_ffi.h).
//
// Chiamabile da BASIC con:
//   DECLARE FUNCTION MathAdd LIB "./NativeGo/math_go" (a AS LONG, b AS LONG) AS LONG
//
// Regole:
//   - `//export` + `-buildmode=c-shared` = ABI C pura (scalari + *C.char);
//   - stringhe OUT: C.CString (alloca in C heap) — GMBasic copia subito, ma il
//     buffer C resta allocato → esporre math_label_free (C.free);
//   - stringhe IN: C.GoString copia l'input Go (mai tenere *C.char dopo il call).
//
// Build:
//   go build -buildmode=c-shared -o libmath_go.dylib math_go.go
// ============================================================================

package main

/*
#include <stdlib.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
)

//export math_add
func math_add(a int64, b int64) int64 { return a + b }

//export math_scale
func math_scale(a float64, b float64) float64 { return a * b }

//export math_isprime
func math_isprime(n int64) int64 {
	if n < 2 {
		return 0
	}
	for i := int64(2); i*i <= n; i++ {
		if n%i == 0 {
			return 0
		}
	}
	return 1
}

//export math_factorial
func math_factorial(n int64) int64 {
	r := int64(1)
	for k := int64(2); k <= n; k++ {
		r *= k
	}
	return r
}

//export math_label
func math_label(id int64, name *C.char) *C.char {
	return C.CString(fmt.Sprintf("%d:%s", id, C.GoString(name)))
}

//export math_label_free
func math_label_free(s *C.char) {
	C.free(unsafe.Pointer(s))
}

func main() {}
