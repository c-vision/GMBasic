/* gmb_ai_register.c — constructor della dylib `gmb_runtime_ai`.
 *
 * Registra nel core i servizi ABI dei modelli (LLMEnv `llmdesc`): il core non linka LLMEnv
 * (SPLIT 2026-11-02) e senza questa registrazione le operazioni che leggono `.gmbarch`
 * riporterebbero "ABI assente". Gira al dlopen della dylib (prima di ogni uso AI).
 */
#include <stddef.h>

extern int  llmdesc_abi_valid(const char* dir);
extern int  llmdesc_abi_locate(char* out, size_t cap, char* how, size_t howcap);
extern void llmdesc_set_abi_policy(int (*resolver)(char* out, size_t cap), int required);
extern void gmb_abi_set_llmdesc(int (*valid)(const char*),
                                int (*locate)(char*, size_t, char*, size_t),
                                void (*policy)(int (*)(char*, size_t), int));

__attribute__((constructor)) static void gmb_runtime_ai_register(void)
{
    gmb_abi_set_llmdesc(llmdesc_abi_valid, llmdesc_abi_locate, llmdesc_set_abi_policy);
}
