/* credential_vault.h — CredentialVault per BROWSER.LOGIN (CUA §25)
 * Il modello non vede mai password/cookie/token — il vault li tiene e il
 * BrowserProvider apre la sessione. Un file/dominio. */
#ifndef GMB_CREDENTIAL_VAULT_H
#define GMB_CREDENTIAL_VAULT_H

int gmb_credential_vault_store(const char *service, const char *user, const char *secret);
const char *gmb_credential_vault_load(const char *service, const char *user); // ritorna secret o NULL
int gmb_credential_vault_remove(const char *service, const char *user);
int gmb_credential_vault_has(const char *service);

#endif
