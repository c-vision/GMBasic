/* gmb_members.h — MEMBER-TABLE condivisa dell'emitter C99Native (task: rimozione
 * gmb_token_names/from_name, direttiva 2026-09-08).
 * Risolve la GRAFIA del membro dopo il punto (dict.Keys, list.Peek, obj.Send, ...) nel
 * TokenKind membro, SENZA passare da gmb_token_names (disallineata all'enum). Lookup
 * locale, non oracolo globale. Condivisa tra gmb_c99_emit.c e gmb_c99_emit_builtin.c
 * (prima viveva statica in gmb_c99_emit.c). */
#ifndef GMB_MEMBERS_H
#define GMB_MEMBERS_H

#include "gmb_tokenkind.h"
#include <strings.h>
#include <stddef.h>   /* NULL + size_t: su glibc <strings.h> non li garantisce (macOS sì) */

typedef struct { const char* k; gmb_tokenkind_t tk; } gmb_member_row_t;
static const gmb_member_row_t gmb_members[] = {

    { "Add", GMB_TOK_Add },
    { "AddAfter", GMB_TOK_CollAddAfter },
    { "AddBefore", GMB_TOK_CollAddBefore },
    { "AddFirst", GMB_TOK_AddFirst },
    { "AddLast", GMB_TOK_AddLast },
    { "AddNode", GMB_TOK_AddNode },
    { "AddEdge", GMB_TOK_CollAddEdge },
    { "AddNodeConsistent", GMB_TOK_CollAddNodeConsistent },
    { "AddRange", GMB_TOK_AddRange },
    { "AddUnique", GMB_TOK_CollAddUnique },
    { "Back", GMB_TOK_CollBack },
    { "Body", GMB_TOK_Body },
    { "BFS", GMB_TOK_CollBFS },
    { "Capacity", GMB_TOK_CollCapacity },
    { "Clear", GMB_TOK_Clear },
    { "Close", GMB_TOK_Close },
    { "Contains", GMB_TOK_Contains },
    { "ContainsKey", GMB_TOK_ContainsKey },
    { "Count", GMB_TOK_Count },
    { "Current", GMB_TOK_Current },
    { "Dequeue", GMB_TOK_Dequeue },
    { "Description", GMB_TOK_Description },
    { "Dijkstra", GMB_TOK_CollDijkstra },
    { "DFS", GMB_TOK_CollDFS },
    { "Enqueue", GMB_TOK_Enqueue },
    { "ErrorMessage", GMB_TOK_ErrorMessage },
    { "Except", GMB_TOK_Except },
    { "Extract", GMB_TOK_Extract },
    { "ExtractMax", GMB_TOK_ExtractMax },
    { "ExtractMin", GMB_TOK_ExtractMin },
    { "FindAll", GMB_TOK_FindAll },
    { "First", GMB_TOK_First },
    { "Front", GMB_TOK_CollFront },
    { "Get", GMB_TOK_Get },
    { "GetEdgeWeight", GMB_TOK_CollGetEdgeWeight },
    { "GetInDegree", GMB_TOK_CollGetInDegree },
    { "GetNeighbors", GMB_TOK_CollGetNeighbors },
    { "GetNode", GMB_TOK_GetNode },
    { "GetRange", GMB_TOK_CollGetRange },
    { "HasEdge", GMB_TOK_HasEdge },
    { "HasItem", GMB_TOK_HasItem },
    { "HasNode", GMB_TOK_HasNode },
    { "IndexOf", GMB_TOK_IndexOf },
    { "Insert", GMB_TOK_Insert },
    { "Intersect", GMB_TOK_Intersect },
    { "Ip", GMB_TOK_Ip },
    { "IsEmpty", GMB_TOK_CollIsEmpty },
    { "Join", GMB_TOK_Join },
    { "Keys", GMB_TOK_Keys },
    { "Last", GMB_TOK_Last },
    { "LineNumber", GMB_TOK_LineNumber },
    { "Max", GMB_TOK_Max },
    { "MaxPair", GMB_TOK_CollMaxPair },
    { "Method", GMB_TOK_Method },
    { "Min", GMB_TOK_Min },
    { "MinPair", GMB_TOK_CollMinPair },
    { "Nodes", GMB_TOK_CollNodes },
    { "New", GMB_TOK_New },
    { "Nil", GMB_TOK_Nil },
    { "NodeCount", GMB_TOK_NodeCount },
    { "Number", GMB_TOK_Number },
    { "OnClose", GMB_TOK_OnClose },
    { "OnError", GMB_TOK_OnError },
    { "OnMessage", GMB_TOK_OnMessage },
    { "Path", GMB_TOK_Path },
    { "Peek", GMB_TOK_Peek },
    { "Pop", GMB_TOK_Pop },
    { "Push", GMB_TOK_Push },
    { "Query", GMB_TOK_Query },
    { "QueryRange", GMB_TOK_QueryRange },
    { "Remove", GMB_TOK_Remove },
    { "RemoveAt", GMB_TOK_RemoveAt },
    { "RemoveFirst", GMB_TOK_RemoveFirst },
    { "RemoveLast", GMB_TOK_RemoveLast },
    { "RemoveNode", GMB_TOK_RemoveNode },
    { "RemoveEdge", GMB_TOK_CollRemoveEdge },
    { "Reverse", GMB_TOK_Reverse },
    { "Rotate", GMB_TOK_Rotate },
    { "Send", GMB_TOK_Send },
    { "Set", GMB_TOK_CollSet },
    { "SetKeyValue", GMB_TOK_CollSetKeyValue },
    { "Sort", GMB_TOK_Sort },
    { "StartsWith", GMB_TOK_StartsWith },
    { "StatusText", GMB_TOK_StatusText },
    { "Sum", GMB_TOK_CollSum },
    { "ToArray", GMB_TOK_ToArray },
    { "ToDict", GMB_TOK_ToDict },
    { "ToJson", GMB_TOK_ToJson },
    { "ToList", GMB_TOK_ToList },
    { "TryGet", GMB_TOK_CollTryGet },
    { "TryGetValue", GMB_TOK_CollTryGetValue },
    { "Union", GMB_TOK_Union },
    { "UniqueCount", GMB_TOK_UniqueCount },
    { "UniqueItems", GMB_TOK_UniqueItems },
    { "Update", GMB_TOK_Update },
    { "Url", GMB_TOK_Url },
    { "Values", GMB_TOK_Values },
};

/* Grafia membro → TokenKind; 0 se non noto (i membri dinamici futuri restano per nome). */
static inline gmb_tokenkind_t gmb_member_tok(const char* m)
{
    if (!m) return 0;
    for (size_t i = 0; i < sizeof(gmb_members) / sizeof(gmb_members[0]); i++)
        if (strcasecmp(m, gmb_members[i].k) == 0) return gmb_members[i].tk;
    return (gmb_tokenkind_t)0;
}

/* F1e (regola d'oro): TokenKind → NOME CANONICO (grafia della prima row col kind):
 * il chunk OP_CALL_MEMBER trasporta QUESTA forma (una sola per kind) su cui VM/C
 * strcmp e PCode dispatchano. NULL se il kind non è un membro noto (lexeme raw). */
static inline const char* gmb_member_canon(gmb_tokenkind_t tk)
{
    if (!tk) return NULL;
    for (size_t i = 0; i < sizeof(gmb_members) / sizeof(gmb_members[0]); i++)
        if (gmb_members[i].tk == tk) return gmb_members[i].k;
    return NULL;
}

/* F1 (piano Incongruenze): i TokenKind Coll* dedicati dei metodi di collection.
 * NOTA F1e/F1g: tutti i Coll* dedicati ora hanno implementazione sui 4 backend
 * (AddNodeConsistent incluso, commit 7faf30043 — runtime condiviso
 * gmb_graph_add_node_consistent). MinPair/MaxPair/SetKeyValue sono gestiti in tutti i backend
 * (F1 Incongruenze: IL DictMaxMinPair + PCode dict branch + VMC gmb_vm_core + C99/C99Native). */
static const gmb_tokenkind_t gmb_coll_member_toks[] = {
    GMB_TOK_CollCount, GMB_TOK_CollAdd, GMB_TOK_CollAddUnique, GMB_TOK_CollAddRange,
    GMB_TOK_CollInsert, GMB_TOK_CollRemoveAt, GMB_TOK_CollRemove, GMB_TOK_CollClear,
    GMB_TOK_CollContains, GMB_TOK_CollIndexOf, GMB_TOK_CollJoin, GMB_TOK_CollSort,
    GMB_TOK_CollReverse, GMB_TOK_CollMax, GMB_TOK_CollMin, GMB_TOK_CollSum,
    GMB_TOK_CollUnion, GMB_TOK_CollIntersect, GMB_TOK_CollExcept, GMB_TOK_CollToArray,
    GMB_TOK_CollToList, GMB_TOK_CollPush, GMB_TOK_CollPop, GMB_TOK_CollPeek,
    GMB_TOK_CollEnqueue, GMB_TOK_CollDequeue, GMB_TOK_CollFirst, GMB_TOK_CollLast,
    GMB_TOK_CollAddAfter, GMB_TOK_CollAddBefore, GMB_TOK_CollAddFirst, GMB_TOK_CollAddLast,
    GMB_TOK_CollRemoveFirst, GMB_TOK_CollRemoveLast, GMB_TOK_CollUniqueCount,
    GMB_TOK_CollUniqueItems, GMB_TOK_CollHasItem, GMB_TOK_CollStartsWith, GMB_TOK_CollFindAll,
    GMB_TOK_CollCapacity, GMB_TOK_CollKeys, GMB_TOK_CollValues, GMB_TOK_CollSet,
    GMB_TOK_CollContainsKey, GMB_TOK_CollGet, GMB_TOK_CollTryGetValue, GMB_TOK_CollTryGet,
    GMB_TOK_CollToDict, GMB_TOK_CollExtractMin, GMB_TOK_CollExtractMax, GMB_TOK_CollRotate,
    GMB_TOK_CollCurrent, GMB_TOK_CollUpdate, GMB_TOK_CollQuery, GMB_TOK_CollQueryRange,
    GMB_TOK_CollAddNode, GMB_TOK_CollAddEdge, GMB_TOK_CollHasNode, GMB_TOK_CollHasEdge,
    GMB_TOK_CollRemoveNode, GMB_TOK_CollRemoveEdge, GMB_TOK_CollGetEdgeWeight,
    GMB_TOK_CollGetInDegree, GMB_TOK_CollNodeCount, GMB_TOK_CollGetNeighbors, GMB_TOK_CollBFS,
    GMB_TOK_CollDFS, GMB_TOK_CollDijkstra, GMB_TOK_CollNodes, GMB_TOK_CollGetRange,
    GMB_TOK_CollToJSON, GMB_TOK_CollFront, GMB_TOK_CollBack, GMB_TOK_CollIsEmpty,
};

/* True se il TokenKind è un metodo di collection (Coll*) gestito dal VM/C. */
static inline int gmb_is_coll_member_tok(gmb_tokenkind_t tk)
{
    for (size_t i = 0; i < sizeof(gmb_coll_member_toks) / sizeof(gmb_coll_member_toks[0]); i++)
        if (tk == gmb_coll_member_toks[i]) return 1;
    return 0;
}

#endif /* GMB_MEMBERS_H */
