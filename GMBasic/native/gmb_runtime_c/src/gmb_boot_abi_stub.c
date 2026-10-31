/* gmb_boot_abi_stub.c — ANCORA dell'ABI per `libgmb_boot`.
 *
 * `gmb_paths.c` (incluso in libgmb_boot per il parser delle impostazioni) riferisce
 * `gmb_abi_link_anchor`: nei runtime normali serve a forzare l'oggetto `gmb_abi.c` dentro il link
 * statico. In `libgmb_boot` NON vogliamo la registry ABI (tira `llmdesc.h` e quindi mezza catena
 * LLMEnv): questa lib deve restare con dipendenze minime — libSystem + libcurl — perché il C# la
 * carica PRIMA di ogni altra. Lo stub soddisfa il riferimento e non fa nulla: la registry vera
 * vive nel runtime completo, dove serve.
 */
void gmb_abi_link_anchor(void) { }
