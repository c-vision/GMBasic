/* capability.c — implementazione Policy */
#include "capability.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>

#define MAX_AGENTS 32
#define MAX_TOOLS 32

typedef struct { char tool[64]; char policy[16]; } tool_policy_t;
typedef struct { char agent[64]; tool_policy_t tools[MAX_TOOLS]; int count; } agent_policy_t;

static agent_policy_t g_policies[MAX_AGENTS];
static int g_policy_count = 0;

static agent_policy_t *find_or_create(const char *agent) {
    for (int i = 0; i < g_policy_count; i++) if (strcasecmp(g_policies[i].agent, agent) == 0) return &g_policies[i];
    if (g_policy_count >= MAX_AGENTS) return NULL;
    strncpy(g_policies[g_policy_count].agent, agent, 63);
    g_policies[g_policy_count].count = 0;
    return &g_policies[g_policy_count++];
}

int gmb_capability_set(const char *agent, const char *tool, const char *policy) {
    if (!agent || !tool || !policy) return -1;
    if (strcasecmp(policy, "allow") != 0 && strcasecmp(policy, "deny") != 0 && strcasecmp(policy, "approve") != 0) return -1;
    agent_policy_t *ap = find_or_create(agent);
    if (!ap) return -1;
    for (int i = 0; i < ap->count; i++) if (strcasecmp(ap->tools[i].tool, tool) == 0) {
        strncpy(ap->tools[i].policy, policy, 15);
        return 0;
    }
    if (ap->count >= MAX_TOOLS) return -1;
    strncpy(ap->tools[ap->count].tool, tool, 63);
    strncpy(ap->tools[ap->count].policy, policy, 15);
    ap->count++;
    return 0;
}

gmb_cap_verdict_t gmb_capability_check(const char *agent, const char *tool) {
    if (!agent || !tool) return GMB_CAP_ALLOW;
    for (int i = 0; i < g_policy_count; i++) if (strcasecmp(g_policies[i].agent, agent) == 0) {
        for (int j = 0; j < g_policies[i].count; j++) if (strcasecmp(g_policies[i].tools[j].tool, tool) == 0) {
            if (strcasecmp(g_policies[i].tools[j].policy, "deny") == 0) return GMB_CAP_DENY;
            if (strcasecmp(g_policies[i].tools[j].policy, "approve") == 0) return GMB_CAP_APPROVE;
            return GMB_CAP_ALLOW;
        }
    }
    return GMB_CAP_ALLOW; // default YOLO (parità AgentPermissionService)
}

int gmb_capability_clear(const char *agent) {
    for (int i = 0; i < g_policy_count; i++) if (strcasecmp(g_policies[i].agent, agent) == 0) {
        for (int j = i; j < g_policy_count-1; j++) g_policies[j] = g_policies[j+1];
        g_policy_count--;
        return 0;
    }
    return -1;
}
