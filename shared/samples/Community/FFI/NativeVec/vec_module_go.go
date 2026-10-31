// ============================================================================
// vec_module_go.go — valutazione modulo nativo GMBasic in Go (cgo)
// File: samples/Community/FFI/NativeVec/vec_module_go.go
//
// Go con cgo espone funzioni C ABI SOLO scalari (int64/float64) e richiede
// `-buildmode=c-shared` + `//export`. NON può includere gmb_ffi.h direttamente;
// cgo genera un header `libvec_go.h` con le dichiarazioni `//export`.
//
//   LIVELLO 1 (c-abi, DECLARE): funzioni `//export vec_add ...` scalari.
//     Le stringhe richiedono C.CString (copia) e C.GoString per leggere input.
//     IMPORTANTE: C.CString alloca in C heap — l'output va liberato con
//     C.free (GMBasic copia subito, quindi restituire un CString e lasciare
//     che il runtime copi è ok se si accetta un leak; meglio esporre *_free).
//
//   LIVELLO 2 (gmb-native): Go NON può ricevere `gmb_ffi_val_t` BY VALUE
//     (le struct C by-value non sono supportate nei `//export`); si passano
//     `*C.gmb_ffi_val_t` (cgo permette puntatori a struct C via preamble). Il
//     mirroring di gmb_ffi.h richiede il blocco `import "C"` con i tipi.
//
// Build (lib):
//   go build -buildmode=c-shared -o libvec_go.dylib vec_module_go.go
// ============================================================================

package main

/*
#include <stdlib.h>
// Mirror minimo di gmb_ffi.h per il livello 2 (puntatori, non by-value).
typedef struct gmb_ffi_val_mirror {
    int kind;
    int pad;
    long long data; // union: int/double/bool/ptr
} gmb_ffi_val_mirror;
*/
import "C"

import (
	"fmt"
	"unsafe"
)

// --- Livello 1: scalari -------------------------------------------------------

//export vec_add
func vec_add(a int64, b int64) int64 { return a + b }

//export vec_scale
func vec_scale(a float64, b float64) float64 { return a * b }

//export vec_isprime
func vec_isprime(n int64) int64 {
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

//export vec_label
func vec_label(id int64, name *C.char) *C.char {
	// C.GoString legge il C string input (copia in Go).
	// C.CString alloca in C heap: GMBasic copia subito (copy-out), ma il
	// buffer C resterebbe allocato → in un modulo reale esporre vec_label_free.
	return C.CString(fmt.Sprintf("%d:%s", id, C.GoString(name)))
}

//export vec_label_free
func vec_label_free(s *C.char) { C.free(unsafe.Pointer(s)) }

// --- Livello 2 (gmb-native): puntatori a mirror, MAI by-value -----------------

//export vec_native_add
func vec_native_add(a *C.gmb_ffi_val_mirror, b *C.gmb_ffi_val_mirror, out *C.gmb_ffi_val_mirror) {
	// placeholder: il marshalling reale leggerebbe kind/data dei puntatori.
	out.data = C.longlong(0)
}

func main() {}
