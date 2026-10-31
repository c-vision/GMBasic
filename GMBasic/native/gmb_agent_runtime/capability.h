/* capability.h — Capability/Policy per agent (CUA §26, §15)
 * Ogni agente ha una policy ALLOW/DENY/APPROVE per tool.
 * Un file/dominio, <1200 righe. */
#ifndef GMB_CAPABILITY_H
#define GMB_CAPABILITY_H

typedef enum { GMB_CAP_ALLOW = 0, GMB_CAP_DENY = 1, GMB_CAP_APPROVE = 2 } gmb_cap_verdict_t;

int gmb_capability_set(const char *agent, const char *tool, const char *policy); // "allow"|"deny"|"approve"
gmb_cap_verdict_t gmb_capability_check(const char *agent, const char *tool); // default ALLOW (YOLO) se non settato
int gmb_capability_clear(const char *agent);

#endif
