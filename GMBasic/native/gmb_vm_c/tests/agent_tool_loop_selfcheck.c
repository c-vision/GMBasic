/* FILE: native/gmb_vm_c/tests/agent_tool_loop_selfcheck.c
 * Verifica DETERMINISTICA del tool-loop di AI AGENT ASK (parità AgentToolLoop.AskWithTools
 * C#), senza LLM reale: brain finto scriptato + tool del registry con executor esterno.
 * Copre: (a) il tool viene eseguito, (b) l'osservazione rientra nel round successivo,
 * (c) il loop termina col testo finale, (d) il cap round è rispettato, (e) loop identico
 * interrotto, (f) agente SENZA tool → single-ask invariato. */
#include "gmb_runtime.h"
#include "gmb_agent_loop.h"
#include "tool_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;
static int g_brain_mode = 0;   /* 0=tool poi finale; 1=sempre finale; 2=tool sempre (cap); 3=identico */
static int g_brain_calls = 0;
static int g_echo_calls = 0;
static int g_obs_in_transcript = 0;

static int fake_brain(void* ctx, const char* transcript, char* out, size_t cap) {
    (void)ctx;
    g_brain_calls++;
    if (g_brain_mode == 0) {
        if (g_brain_calls == 1) {
            snprintf(out, cap,
                "{\"tool_calls\":[{\"name\":\"echo\",\"arguments\":{\"text\":\"ciao\"}}]}");
        } else {
            if (strstr(transcript, "echoed")) g_obs_in_transcript = 1;
            snprintf(out, cap, "FINAL: ok");
        }
    } else if (g_brain_mode == 1) {
        snprintf(out, cap, "SINGLE: no tools");
    } else if (g_brain_mode == 2) {
        /* args diversi ad ogni round: non deve scattare il loop-detector "identico". */
        snprintf(out, cap,
            "{\"tool_calls\":[{\"name\":\"echo\",\"arguments\":{\"text\":\"r%d\"}}]}", g_brain_calls);
    } else {
        snprintf(out, cap,
            "{\"tool_calls\":[{\"name\":\"echo\",\"arguments\":{\"text\":\"same\"}}]}");
    }
    return 0;
}

static int echo_exec(const char* name, const char* args_json, char* out, size_t cap, void* ctx) {
    (void)name; (void)ctx;
    g_echo_calls++;
    snprintf(out, cap, "echoed %s", args_json);
    return 0;
}

static void check(const char* what, int cond) {
    printf("%s %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) g_fail = 1;
}

int main(void) {
    gmb_agent_brain_register(fake_brain, NULL);
    gmb_tool_registry_clear();
    gmb_tool_registry_register_simple("echo", "Echo un testo", "test");
    gmb_tool_registry_set_executor(echo_exec, NULL);

    /* (1) agente con tool ESPLICITO → tool-loop reale. */
    gmb_agent_create("agentT");
    gmb_agent_register_tool("agentT", "echo");
    g_brain_mode = 0; g_brain_calls = 0; g_echo_calls = 0; g_obs_in_transcript = 0;
    char* a = gmb_agent_ask("agentT", "usa echo per salutare");
    check("(a) tool eseguito dall'executor", g_echo_calls == 1);
    check("(b) osservazione nel transcript del round successivo", g_obs_in_transcript == 1);
    check("(c) loop termina col testo finale", a && strcmp(a, "FINAL: ok") == 0);
    check("    brain chiamato 2 volte (tool + finale)", g_brain_calls == 2);
    free(a);

    /* (2) agente SENZA tool → single ask invariato (prompt senza trigger plugin). */
    gmb_agent_create("agentN");
    g_brain_mode = 1; g_brain_calls = 0;
    char* b = gmb_agent_ask("agentN", "qxz");
    check("(f) nessun tool: single ask invariato", b && strcmp(b, "SINGLE: no tools") == 0);
    check("    nessun tool: brain chiamato 1 volta", g_brain_calls == 1);
    free(b);

    /* (3) cap round: brain che chiede sempre un tool (args variabili) → cap a 5. */
    gmb_agent_create("agentC");
    gmb_agent_register_tool("agentC", "echo");
    g_brain_mode = 2; g_brain_calls = 0; g_echo_calls = 0;
    char* c = gmb_agent_ask("agentC", "loop");
    check("(d) cap round rispettato", c && strstr(c, "exceeded 5 rounds") != NULL);
    check("    brain chiamato = cap (5)", g_brain_calls == 5);
    check("    executor chiamato 5 volte", g_echo_calls == 5);
    free(c);

    /* (4) risposta identica → loop detector, mai infinito. */
    gmb_agent_create("agentD");
    gmb_agent_register_tool("agentD", "echo");
    g_brain_mode = 3; g_brain_calls = 0;
    char* d = gmb_agent_ask("agentD", "loop identico");
    check("(e) loop identico interrotto", d && strstr(d, "Loop rilevato") != NULL);
    free(d);

    gmb_tool_registry_set_executor(NULL, NULL);
    gmb_agent_brain_clear();
    printf(g_fail ? "\n== FAIL ==\n" : "\n== ALL PASS ==\n");
    return g_fail;
}
