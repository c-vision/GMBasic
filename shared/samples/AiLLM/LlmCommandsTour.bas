' FILE: AiLLM/LlmCommandsTour.bas
' =====================================================================================
' TOUR COMPLETO dei comandi LLM aggiunti/modificati — usa OGNI comando/funzione.
'   REPORT (§4.2-§4.10): su work/dsv2l_mxfp4.gguf (ha general.architecture -> CHECK/arch ok).
'   SURGERY: su ../../Fixtures/models/tiny_moe.gguf (fast).
' Copre: LLM TENSOR IR/INFO/LAYERS/CHECK/MEMORY/COST/GRAPH/SPEC/KV/STATE/MOBJ/CONTEXT/OBUDGET/CONTEXTGC/CONTEXTMAP/CAPACITY/DECODEPROFILE/MPLAN/EXPOLICY/SPEC/STATE/KV/
'        GPU/PROFILE/DIST/EPLAN/SCHEDULE/CED/EXECPLAN/GEXPORT/COMPILE/LOADPLAN,
'        LLM COMPILE, LLM MODEL CHECKPOINT/ABLATE/DIFF/VALIDATE/SAVE/ADD/ROLLBACK.
' NOTA: gli argomenti dei comandi LLM sono STRINGHE LETTERALI (niente variabili).
' Prerequisito: work/dsv2l_mxfp4.gguf (catena ModelCompilerMlxToDs4.bas) + fixture tiny_moe.
' =====================================================================================
IMPORT LLM

SUB Main()
    MKDIR "work"
    IF NOT IO.FILEEXISTS("work/dsv2l_mxfp4.gguf") THEN
        PRINT "assente work/dsv2l_mxfp4.gguf (esegui prima ModelCompilerMlxToDs4.bas)"
        END
    END IF

    ' ── MODEL INSPECTOR / IR / Planner / GPU (report-only, arch=deepseek_v2) ─────────
    LLM TENSOR IR "work/dsv2l_mxfp4.gguf"
    LLM TENSOR INFO "work/dsv2l_mxfp4.gguf"
    LLM TENSOR LAYERS "work/dsv2l_mxfp4.gguf"
    LLM TENSOR CHECK "work/dsv2l_mxfp4.gguf"
    LLM TENSOR MEMORY "work/dsv2l_mxfp4.gguf"
    LLM TENSOR COST "work/dsv2l_mxfp4.gguf"
    LLM TENSOR GRAPH "work/dsv2l_mxfp4.gguf"
    LLM TENSOR SPEC "work/dsv2l_mxfp4.gguf"
    LLM TENSOR KV "work/dsv2l_mxfp4.gguf"
    LLM TENSOR STATE "work/dsv2l_mxfp4.gguf"
    LLM TENSOR MOBJ "work/dsv2l_mxfp4.gguf"
    LLM TENSOR CONTEXT "work/dsv2l_mxfp4.gguf"
    LLM TENSOR GPU "work/dsv2l_mxfp4.gguf"
    LLM TENSOR PROFILE "work/dsv2l_mxfp4.gguf"
    LLM TENSOR DIST "work/dsv2l_mxfp4.gguf"
    LLM TENSOR EPLAN "work/dsv2l_mxfp4.gguf"
    LLM TENSOR SCHEDULE "work/dsv2l_mxfp4.gguf"
    LLM TENSOR CED "work/dsv2l_mxfp4.gguf"
    LLM TENSOR QUANTIR "work/dsv2l_mxfp4.gguf"
    LLM TENSOR OBUDGET "work/dsv2l_mxfp4.gguf", "O2"
    LLM TENSOR MEMPLAN "work/dsv2l_mxfp4.gguf"
    LLM TENSOR KVPLAN "work/dsv2l_mxfp4.gguf"
    LLM TENSOR VIEW "work/dsv2l_mxfp4.gguf"
    LLM TENSOR POLICYIR "work/dsv2l_mxfp4.gguf"
    LLM TENSOR IRLEVELS "work/dsv2l_mxfp4.gguf"
    LLM TENSOR PHASE "work/dsv2l_mxfp4.gguf", "1024"
    LLM TENSOR CONTEXT "work/dsv2l_mxfp4.gguf"
    LLM TENSOR CONTEXTGC "work/dsv2l_mxfp4.gguf", "8192:4096"
    LLM TENSOR CONTEXTMAP "work/dsv2l_mxfp4.gguf", "16384"
    LLM TENSOR CAPACITY "work/dsv2l_mxfp4.gguf", "8192"
    LLM TENSOR DECODEPROFILE "work/dsv2l_mxfp4.gguf", "8192:450"
    LLM TENSOR MPLAN "work/dsv2l_mxfp4.gguf", "5"
    LLM TENSOR EXPOLICY "work/dsv2l_mxfp4.gguf", "5"
    LLM TENSOR SPEC "work/dsv2l_mxfp4.gguf"
    LLM TENSOR STATE "work/dsv2l_mxfp4.gguf"
    LLM TENSOR KV "work/dsv2l_mxfp4.gguf", "PAGED"

    ' ── Artefatti serializzabili (EXECPLAN / GEXPORT / COMPILE / LOADPLAN) ──────────
    LLM TENSOR EXECPLAN "work/dsv2l_mxfp4.gguf", "work/tour_plan.json"
    LLM TENSOR GEXPORT "work/dsv2l_mxfp4.gguf", "work/tour_model.dot"
    LLM COMPILE "work/dsv2l_mxfp4.gguf", "work/tour.gmbllm"
    LLM TENSOR LOADPLAN "work/tour.gmbllm"
    LLM TENSOR COMPILE "work/dsv2l_mxfp4.gguf", "work/tour2.gmbllm"

    ' ── SURGERY su file (fixture tiny, fast) ────────────────────────────────────────
    LLM MODEL CHECKPOINT "../Fixtures/models/tiny_moe.gguf", "work/tour_ck", "orig"
    LLM MODEL ABLATE "../Fixtures/models/tiny_moe.gguf", "work/tour_abl.gguf", "blk.1"
    LLM MODEL REMOVE "../Fixtures/models/tiny_moe.gguf", "work/tour_rm.gguf", 1
    LLM MODEL DIFF "../Fixtures/models/tiny_moe.gguf", "work/tour_abl.gguf", "work/tour_diff.txt"
    LLM MODEL VALIDATE "work/tour_abl.gguf", "work/tour_val.txt"
    LLM MODEL SAVE "work/tour_abl.gguf", "work/tour_saved.gguf"
    LLM MODEL ADD "../Fixtures/models/tiny_moe.gguf", "work/tour_abl.gguf", "work/tour_add.gguf"
    LLM MODEL ROLLBACK "work/tour_ck", "orig", "work/tour_restored.gguf"

    PRINT "OK: tour completo dei comandi LLM."
END SUB
