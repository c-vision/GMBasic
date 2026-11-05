#include "f2c.h"
#include <math.h>
doublereal d1mach_(integer *i) {
    switch (*i) {
    case 1: return 2.2250738585072014e-308;   /* smallest positive magnitude */
    case 2: return 1.7976931348623157e308;    /* largest magnitude */
    case 3: return 1.1102230246251565e-16;    /* smallest relative spacing B**(-P) */
    case 4: return 2.2204460492503131e-16;    /* largest relative spacing B**(1-P) */
    case 5: return 0.30102999566398120;       /* log10(2) */
    default: return 0.0;
    }
}
integer i1mach_(integer *i) {
    switch (*i) {
    case 9:  return 2147483647;  /* largest integer */
    case 14: return 53;          /* mantissa double */
    case 15: return -1021;       /* min exponent double */
    case 16: return 1024;        /* max exponent double */
    default: return 0;
    }
}
