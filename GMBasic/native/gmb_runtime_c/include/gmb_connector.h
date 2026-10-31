#pragma once
// FILE: gmb_connector.h — Connector Runtime F0 (spec MCP SYSTEM.txt §24-35)
// Runtime C condiviso per i 52 connettori (T0-T4), 8 primitive: CONNECTOR/AUTH/TOOL/RESOURCE/POLICY/APPROVAL/SECRET/SESSION
// Parità C# (Basic.Connectors.*) — unica implementazione, VM/C e C99 la chiamano.

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

// Ritorna JSON string (malloc, caller free) o NULL su errore. Tutti i metodi sono thread-safe (file lock).
char* gmb_connector_list(void);
char* gmb_connector_status(const char* name);
char* gmb_connector_tools(const char* name);
char* gmb_connector_search(const char* capability);
char* gmb_connector_marketplace_list(void);
char* gmb_connector_marketplace_get(const char* name);
int   gmb_connector_register(const char* name, const char* kind, const char* config_json);
int   gmb_connector_unregister(const char* name);
int   gmb_connector_load(const char* name);
int   gmb_connector_enable(const char* name, int enabled);
char* gmb_connector_call(const char* connector, const char* tool, const char* args_json);
int   gmb_connector_refresh(const char* name);
char* gmb_connector_policy_get(const char* connector, const char* tool);
int   gmb_connector_policy_set(const char* connector, const char* tool, const char* policy);
char* gmb_connector_secret_get(const char* connector, const char* key);
int   gmb_connector_secret_set(const char* connector, const char* key, const char* value);
int   gmb_connector_auth(const char* connector, const char* auth_json);
char* gmb_connector_resources(const char* name);
char* gmb_connector_prompts(const char* name);

// OAuth per-connector (PKCE)
char* gmb_connector_oauth_create_verifier(int length);
char* gmb_connector_oauth_create_challenge(const char* verifier);
char* gmb_connector_oauth_build_auth_url(const char* authorize_endpoint, const char* client_id, const char* redirect_uri, const char* scope, const char* state, const char* verifier);
char* gmb_connector_oauth_get_auth_header(const char* connector);

#ifdef __cplusplus
}
#endif
