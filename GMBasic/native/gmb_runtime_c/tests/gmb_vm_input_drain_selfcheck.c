/* gmb_vm_input_drain_selfcheck.c — verifica la composizione del drain VM/C:
 * coda input del motore C → gmb_console_sendkey (typeahead) → INKEY/readkey.
 * È la stessa sequenza del loop main di gmb_vm_host.c. Headless, senza SDL. */
#include "gmb_gfx_input.h"
#include "gmb_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

static void drain_once(void)
{
    char key[64];
    while (gmb_gfx_input_poll_key(key, (int)sizeof(key)))
        gmb_console_sendkey(key);
}

int main(void)
{
    gmb_string_t s;

    gmb_gfx_input_reset();

    /* Due tasti in coda → drain → la VM li rilegge in ordine. */
    gmb_gfx_input_push_key("a");
    gmb_gfx_input_push_key("b");
    drain_once();

    memset(&s, 0, sizeof(s));
    int rc = gmb_console_readkey_nb(&s);
    CHECK(rc == 1 && s.length == 1 && s.data && s.data[0] == 'a', "primo tasto dal drain");
    if (s.data) free(s.data);

    memset(&s, 0, sizeof(s));
    rc = gmb_console_readkey_nb(&s);
    CHECK(rc == 1 && s.length == 1 && s.data && s.data[0] == 'b', "secondo tasto dal drain");
    if (s.data) free(s.data);

    /* Stringa multi-char: sendkey inietta l'intera sequenza. */
    gmb_gfx_input_push_key("ok");
    drain_once();
    memset(&s, 0, sizeof(s));
    rc = gmb_console_readkey_nb(&s);
    CHECK(rc == 1 && s.length == 1 && s.data && s.data[0] == 'o', "multi-char 1");
    if (s.data) free(s.data);
    memset(&s, 0, sizeof(s));
    rc = gmb_console_readkey_nb(&s);
    CHECK(rc == 1 && s.length == 1 && s.data && s.data[0] == 'k', "multi-char 2");
    if (s.data) free(s.data);

    /* Coda C vuota → il drain non inietta nulla. */
    drain_once();
    memset(&s, 0, sizeof(s));
    rc = gmb_console_readkey_nb(&s);
    if (s.data) free(s.data);
    (void)rc; /* con schermo grafico off e niente typeahead il valore dipende dal terminale: non assertiamo */

    if (fails == 0) { printf("gmb_vm_input_drain_selfcheck: OK\n"); return 0; }
    printf("gmb_vm_input_drain_selfcheck: %d FAIL\n", fails);
    return 1;
}
