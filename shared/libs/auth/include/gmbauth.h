/* gmbauth.h — API della lib `auth` (shared/libs/auth).
 *
 * JWT HS256 self-contained: nessuna dipendenza esterna (SHA-256/HMAC/base64url
 * implementati in src/auth.c). Le stringhe di ritorno sono char* malloc'd:
 * il bridge FFI le copia e libera.
 */
#ifndef GMBAUTH_H
#define GMBAUTH_H

#ifdef __cplusplus
extern "C" {
#endif

/* Token JWT HS256: base64url(header).base64url(claims).base64url(HMAC-SHA256).
 * header fisso {"alg":"HS256","typ":"JWT"}. NULL su errore. */
char* ofx_auth_jwt_create(const char* claims, const char* secret);

/* 1 se la firma HMAC-SHA256 del token è valida con `secret`, 0 altrimenti. */
int ofx_auth_jwt_verify(const char* token, const char* secret);

/* Payload JSON decodificato (base64url → testo). Stringa vuota su token invalido. */
char* ofx_auth_jwt_payload(const char* token);

#ifdef __cplusplus
}
#endif

#endif /* GMBAUTH_H */
