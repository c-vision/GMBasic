/*
 * gmi_runtime.h — SHIM minimale per i moduli C estratti da GMBasic.
 *
 * I moduli matematici/statistici di GMBasic non dipendono dal runtime VM: usano
 * solo i tipi base. Questo shim fornisce quei tipi senza tirare dentro il
 * runtime completo (nessuna dipendenza da VM/value/GC).
 */
#ifndef GMB_RUNTIME_H
#define GMB_RUNTIME_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef int64_t gmb_int_t;
typedef double gmb_double_t;
typedef bool gmb_bool_t;

/* Convenzione Bit del linguaggio BASIC: -1 = vero, 0 = falso. */
#define GMB_TRUE ((gmb_bool_t)true)
#define GMB_FALSE ((gmb_bool_t)false)

#endif /* GMB_RUNTIME_H */
