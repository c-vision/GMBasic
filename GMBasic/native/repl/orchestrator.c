/* native/repl/orchestrator.c — compilazione .bas -> chunk.
 *
 * F2 (2026-11-01): la compilazione NON avviene più in-process. La REPL invoca il CLI del
 * compilatore (`gmb dump chunk|c`) tramite rpl_gmb_dump()/rpl_gmb_dump_ex() in repl.c.
 * Questo file resta per eventuali helper di orchestrazione lato REPL.
 */
#include "repl.h"
