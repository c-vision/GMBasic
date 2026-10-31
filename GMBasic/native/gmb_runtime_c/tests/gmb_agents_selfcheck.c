/* gmb_agents_selfcheck.c — verifica runtime C dei componenti agent (L1/C): queue, bus,
 * fabric select e knowledge graph. Parità di comportamento coi test C# (AgentQueueManagerTests,
 * KnowledgeGraphTests). Exit 0 = ok. */
#include "gmb_runtime.h"
#include "gmb_graph.h"
#include <stdio.h>
#include <string.h>

static int g_recv = 0;
static void on_msg(const char* ch, const char* from, const char* task, const char* input, void* u){
    (void)ch; (void)from; (void)task; (void)input; (void)u;
    g_recv++;
}

int main(void){
    int fails = 0;
    gmb_graph_init();

    /* ── queue FIFO + pause/resume/stop ── */
    gmb_agent_queue_create("A1");
    gmb_agent_queue_send("A1", "user", "T1", "in1", 0);
    gmb_agent_queue_send("A1", "user", "T2", "in2", 5);
    if(gmb_agent_queue_count("A1") != 2){ printf("FAIL queue count\n"); fails++; }
    char* m = gmb_agent_queue_receive("A1");
    if(!m || !strstr(m, "T1")){ printf("FAIL queue recv T1: %s\n", m ? m : "(null)"); fails++; }
    if(m) free(m);
    gmb_agent_queue_pause("A1");
    if(!gmb_agent_queue_is_paused("A1") || gmb_agent_queue_receive("A1") != NULL){ printf("FAIL pause\n"); fails++; }
    gmb_agent_queue_resume("A1");
    gmb_agent_queue_stop("A1");
    if(!gmb_agent_queue_is_stopped("A1") || gmb_agent_queue_send("A1","user","T3","in3",0)){ printf("FAIL stop\n"); fails++; }

    /* ── bus pub/sub ── */
    gmb_bus_channel_create("ch1");
    gmb_channel_subscribe("ch1", on_msg, NULL);
    gmb_bus_channel_publish("ch1", "hello bus");
    if(g_recv != 1){ printf("FAIL bus publish recv=%d\n", g_recv); fails++; }
    gmb_channel_unsubscribe("ch1", on_msg, NULL);
    gmb_bus_channel_publish("ch1", "second");
    if(g_recv != 1){ printf("FAIL bus unsubscribe\n"); fails++; }

    /* ── fabric select (coordinatore GO SELECT) ── */
    gmb_agent_queue_create("B1");
    gmb_agent_queue_create("B2");
    gmb_agent_queue_send("B2", "user", "Q2", "val2", 0);
    const char* agents[2] = { "B1", "B2" };
    char sel[512];
    int idx = gmb_agent_fabric_select(agents, 2, sel, sizeof(sel), 1000);
    if(idx != 1 || !strstr(sel, "val2")){ printf("FAIL fabric select idx=%d\n", idx); fails++; }

    /* ── knowledge graph (KG VM/C 1436-1438 parity) ── */
    int64_t h = gmb_graph_create();
    if(h < 0){ printf("FAIL graph create\n"); fails++; }
    gmb_graph_add_node(h, "alice"); gmb_graph_add_node(h, "bob"); gmb_graph_add_node(h, "carol");
    gmb_graph_add_edge(h, "alice", "bob", 1.0);
    gmb_graph_add_edge(h, "bob", "carol", 1.0);
    if(!gmb_graph_has_node(h, "bob")){ printf("FAIL kg node\n"); fails++; }
    int n = 0;
    char** path = gmb_graph_shortest_path(h, "alice", "carol", &n);
    if(n != 3 || strcmp(path[0], "alice") || strcmp(path[2], "carol")){ printf("FAIL kg path n=%d\n", n); fails++; }
    if(path) free(path);
    int nn = 0;
    gmb_graph_edge_t* nb = gmb_graph_neighbors(h, "alice", &nn);
    if(nn != 1 || !nb || strcmp(nb[0].node, "bob")){ printf("FAIL kg neighbors nn=%d\n", nn); fails++; }
    if(nb) free(nb);
    gmb_graph_free(h);

    /* ── gmb_kg singleton (parity C# KnowledgeGraphRuntime, path C99/VM 1436-1438) ── */
    if(!gmb_kg_add_node("alice") || !gmb_kg_add_node("bob") || !gmb_kg_add_node("carol")){ printf("FAIL kg add_node\n"); fails++; }
    if(!gmb_kg_add_edge("alice", "bob") || !gmb_kg_add_edge("bob", "carol")){ printf("FAIL kg add_edge\n"); fails++; }
    char* jq = gmb_kg_query("alice");
    if(!jq || !strstr(jq, "bob")){ printf("FAIL kg query node: %s\n", jq ? jq : "(null)"); fails++; }
    if(jq) free(jq);
    char* jp = gmb_kg_query("path alice->carol");
    if(!jp || !strstr(jp, "carol") || !strstr(jp, "alice")){ printf("FAIL kg query path: %s\n", jp ? jp : "(null)"); fails++; }
    if(jp) free(jp);

    /* ── Provider V2 (C): credits/cost/blocked + providers.json (parity M5/ProviderServer) ── */
    gmb_provider_register("completion", "evalp", "fixture", "");
    gmb_ai_set_config("evalp", "credits", "10");
    gmb_provider_record_completion("evalp", 1000);   /* costPerM 0 → decremento 1.0 */
    if(gmb_ai_get_credits("evalp") != 9.0){ printf("FAIL provider credits (got %.6g)\n", gmb_ai_get_credits("evalp")); fails++; }
    gmb_ai_set_credits("evalp", 0);
    gmb_provider_enforce_credits("evalp");
    if(gmb_provider_status_name("evalp") && strcmp(gmb_provider_status_name("evalp"), "BLOCKED") != 0){ printf("FAIL provider blocked\n"); fails++; }
    char jtmp[512]; snprintf(jtmp, sizeof(jtmp), "%s/providers_v2.json", "/tmp");
    if(!gmb_provider_v2_save(jtmp)){ printf("FAIL provider save\n"); fails++; }
    if(gmb_provider_v2_load(jtmp) < 0){ printf("FAIL provider load\n"); fails++; }

    /* ── ON INSERT EXECUTE AGENT (C): trigger su gmb_aiv_index_text (parity AgentInsertTrigger) ── */
    /* Richiede un provider embedding ATTIVO (parità C# ProviderServer/AiVectorRuntime): attiva il
     * fixture registrato di default, altrimenti gmb_aiv_index_text fallisce (nessun trigger). */
    gmb_provider_set_active_explicit("embedding", "fixture-embedding");
    gmb_agent_create("trigA");
    gmb_agent_insert_trigger_register(":memory:", "docs", "trigA", "elabora {text}");
    int iret = gmb_aiv_ensure_collection(":memory:", "docs", 8);
    if(iret){ int ir = gmb_aiv_index_text(":memory:", "docs", "contenuto trigger"); if(!ir) printf("warn index\n"); }
    if(gmb_agent_insert_trigger_fired(":memory:", "docs") < 1){ printf("FAIL insert trigger fire\n"); fails++; }
    gmb_agent_insert_trigger_clear();

    if(fails){ printf("gmb_agents_selfcheck: %d FAIL\n", fails); return 1; }
    printf("gmb_agents_selfcheck: PASS\n");
    return 0;
}
