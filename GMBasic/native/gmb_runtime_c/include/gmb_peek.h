/* gmb_peek.h — PEEK/POKE di memoria (riferimento: GW-BASIC/Engine1999).
 *
 * Perché una memoria VIRTUALE e non quella del processo: PEEK/POKE nel riferimento leggono e
 * scrivono indirizzi assoluti; in un runtime portabile e a quattro gambe (IL/PCode/VM-C/
 * C99Native, con processi diversi e la stessa semantica) un indirizzo assoluto non esiste e
 * sarebbe un crash in attesa. Qui c'è un blocco di 64 KB di proprietà del programma:
 *   - indirizzi 0 .. GMB_PEEK_SIZE-1 (16 bit, come il segmento del riferimento);
 *   - ogni byte vale 0..255: `gmb_poke` maschera il valore (`& 0xFF`) e lo DICE nel commento;
 *   - indirizzo fuori intervallo = errore esplicito (-1), mai un accesso fuori dai limiti.
 *
 * Differenza DICHIARATA dal riferimento: non è la memoria video né quella del sistema; è la
 * memoria del programma BASIC, azzerata all'avvio e riempibile con `gmb_peek_fill`.
 */
#ifndef GMB_PEEK_H
#define GMB_PEEK_H

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_PEEK_SIZE 65536

/* Dimensione dell'area indirizzabile (byte). */
int gmb_peek_size(void);
/* Byte all'indirizzo; -1 se l'indirizzo è fuori dall'area (nessun clamp silenzioso). */
int gmb_peek(unsigned int addr);
/* Scrive un byte (valore mascherato 0..255); 0 = fatto, -1 = indirizzo fuori dall'area. */
int gmb_poke(unsigned int addr, int value);
/* Riempie tutta l'area con un byte (mascherato); 0 = fatto. */
int gmb_peek_fill(int value);

#ifdef __cplusplus
}
#endif

#endif /* GMB_PEEK_H */
