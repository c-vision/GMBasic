/* decision_selfcheck.c — decision layer C (JEV/SystemOne): Mock/Rules, 24 slot, JSON.
 * Model-free: non carica motori. Exit 0 = PASS. */
#include <stdio.h>
#include <string.h>

#include "gmb_ai_decide.h"

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

int main(void) {
    CHECK(sizeof(gmb_decision_slots_noul) / sizeof(gmb_decision_slots_noul[0]) == 2, "NOUL 2 slot");
    CHECK(sizeof(gmb_decision_slots_score) / sizeof(gmb_decision_slots_score[0]) == 6, "SCORE 6 slot");
    CHECK(sizeof(gmb_decision_slots_choice) / sizeof(gmb_decision_slots_choice[0]) == 16, "CHOICE 16 slot");
    CHECK(strcmp(gmb_decision_slots_noul[0].id, "false") == 0, "NOUL slot 0 = false");
    CHECK(strcmp(gmb_decision_slots_choice[15].id, "P") == 0, "CHOICE slot 15 = P");

    gmb_decision_result_t r;

    /* Mock: opzione forzata + validazione NOUL (2 opzioni). */
    gmb_decision_provider_t* mock = gmb_decision_provider_mock("true", 0.9);
    CHECK(mock != NULL, "mock create");
    CHECK(gmb_decision_evaluate(mock, "x", GMB_DECISION_NOUL,
                                gmb_decision_slots_noul, 2, &r) == 1, "mock evaluate");
    CHECK(strcmp(r.option_id, "true") == 0 && r.probability > 0.899 && r.probability < 0.901, "mock forced");
    CHECK(r.has_calibrated == 0 && r.has_confidence == 0, "mock no calibration");
    CHECK(gmb_decision_evaluate(mock, "x", GMB_DECISION_NOUL,
                                gmb_decision_slots_noul, 1, &r) == -1, "NOUL richiede 2 opzioni");
    gmb_decision_provider_free(mock);

    /* Rules: keyword -> true/false. */
    gmb_decision_provider_t* rules = gmb_decision_provider_rules();
    CHECK(rules != NULL, "rules create");
    CHECK(gmb_decision_evaluate(rules, "il cliente ha detto SÌ", GMB_DECISION_NOUL,
                                gmb_decision_slots_noul, 2, &r) == 1, "rules evaluate");
    CHECK(strcmp(r.option_id, "true") == 0 && r.probability == 1.0, "rules yes");
    CHECK(gmb_decision_evaluate(rules, "no, non è fraud", GMB_DECISION_NOUL,
                                gmb_decision_slots_noul, 2, &r) == 1, "rules evaluate no");
    CHECK(strcmp(r.option_id, "false") == 0, "rules no");
    CHECK(gmb_decision_evaluate(rules, "nulla di rilevante", GMB_DECISION_NOUL,
                                gmb_decision_slots_noul, 2, &r) == 1, "rules default");
    CHECK(r.probability == 0.5, "rules fallback 0.5");

    /* SCORE: keyword di severità → slot 5. */
    CHECK(gmb_decision_evaluate(rules, "critical incident", GMB_DECISION_SCORE,
                                gmb_decision_slots_score, 6, &r) == 1, "rules score");
    CHECK(strcmp(r.option_id, "5") == 0 && r.probability == 1.0, "rules score critical");

    /* CHOICE: la label nello stato seleziona lo slot. */
    gmb_decision_option_t ch[3] = {
        { "A", "billing", -1, 0.0f, 1.0f },
        { "B", "technical", -1, 0.0f, 1.0f },
        { "C", "sales", -1, 0.0f, 1.0f },
    };
    CHECK(gmb_decision_evaluate(rules, "technical issue", GMB_DECISION_CHOICE, ch, 3, &r) == 1, "rules choice");
    CHECK(strcmp(r.option_id, "B") == 0 && strcmp(r.value, "technical") == 0 && r.probability == 1.0,
          "rules choice technical");

    gmb_decision_provider_free(rules);

    /* JSON: null per confidence/calibrated e valori numerici quando presenti. */
    char json[512];
    gmb_decision_result_t jr = { GMB_DECISION_NOUL, "true", "true", 0.75, 0, 0, 0, 0 };
    CHECK(gmb_decision_result_json(&jr, json, sizeof(json)) > 0, "json");
    CHECK(strstr(json, "\"kind\":\"NOUL\"") != NULL, "json kind");
    CHECK(strstr(json, "\"option\":\"true\"") != NULL, "json option");
    CHECK(strstr(json, "\"confidence\":null") != NULL, "json confidence null");
    CHECK(strstr(json, "\"calibrated\":null") != NULL, "json calibrated null");
    jr.has_confidence = 1; jr.confidence = 0.8;
    jr.has_calibrated = 1; jr.calibrated = 0.77;
    CHECK(gmb_decision_result_json(&jr, json, sizeof(json)) > 0, "json calibrated");
    CHECK(strstr(json, "\"confidence\":0.800000") != NULL, "json confidence value");
    CHECK(strstr(json, "\"calibrated\":0.770000") != NULL, "json calibrated value");

    printf("DECISION-SELFCHECK %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
