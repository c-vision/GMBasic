/* Generato da gmb_c99_emit (compilatore nativo GMBASIC, FASE 1). */
#include "gmb_runtime.h"
#include "gmb_collections.h"
#include "gmb_finance.h"
#include "gmb_commonlib.h"
#include "gmb_expr.h"
#include "gmb_c99gpm.h"
#include "gmb_network.h"
#include "gmb_msg.h"
#include "gmb_signal.h"
#include "gmb_connector.h"
#include "gmb_graphics.h"
#include "gmb_compress.h"
#include "gmb_grep.h"
#include "gmb_moestream.h"
#include "gmb_cua_runtime.h"
#include "tool_registry.h"
#include "gmb_codegen.h"
#include "gmb_graph.h"
#include "gmb_db.h"
#include "gmb_ml.h"
#include "gmb_torch.h"
#include "llmmodelio_model.h"
#include "gmb_dscope.h"
#include "gmb_uncertainty.h"
#include "gmb_context.h"
#include "gmb_observation.h"
#include "gmb_worker.h"
#include "gmb_tray.h"
#include "gmb_module.h"
#include "gmb_doc.h"
#include "gmb_agent_loop.h"
#include "gmb_md.h"
#include "gmb_bits.h"
#include "gmb_tray.h"
#include "gmb_events.h"
#include "gmb_form.h"
#include "gmb_form_read.h"
#include "gmb_read.h"
#include "gmb_console_extra.h"
#include "gmb_iconsole.h"
#include "gmb_keyboard_extra.h"
#include <unistd.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct gmb_obj_SPRITEINFO {
    gmb_double_t TextureID;
    gmb_double_t X;
    gmb_double_t Y;
    gmb_double_t Rotation;
    gmb_double_t Scale;
    gmb_double_t Color;
} gmb_obj_SPRITEINFO;
static void __gmb_udt_reg_SPRITEINFO(void) {
    gmb_type_register_udt("SPRITEINFO", 6);
    gmb_type_udt_field("SPRITEINFO", "TextureID", (long)offsetof(gmb_obj_SPRITEINFO, TextureID), 1);
    gmb_type_udt_field("SPRITEINFO", "X", (long)offsetof(gmb_obj_SPRITEINFO, X), 1);
    gmb_type_udt_field("SPRITEINFO", "Y", (long)offsetof(gmb_obj_SPRITEINFO, Y), 1);
    gmb_type_udt_field("SPRITEINFO", "Rotation", (long)offsetof(gmb_obj_SPRITEINFO, Rotation), 1);
    gmb_type_udt_field("SPRITEINFO", "Scale", (long)offsetof(gmb_obj_SPRITEINFO, Scale), 1);
    gmb_type_udt_field("SPRITEINFO", "Color", (long)offsetof(gmb_obj_SPRITEINFO, Color), 1);
}
typedef struct { gmb_double_t f0; } gmb_ffi_udtd1_t;
typedef struct { gmb_double_t f0, f1; } gmb_ffi_udtd2_t;
typedef struct { gmb_int_t f0; } gmb_ffi_udti1_t;
typedef struct { gmb_int_t f0, f1; } gmb_ffi_udti2_t;
typedef struct { gmb_double_t f0; gmb_int_t f1; } gmb_ffi_udtdi2_t;
typedef struct { gmb_int_t f0; gmb_double_t f1; } gmb_ffi_udtid2_t;

static gmb_double_t gmb_user_Main(void);

static int __gmb_mcp_exec(void* __ctx, const char* __tool, const char* __proc,
                          const char* __args, char** __out, int* __err);

static char* __gmb_plugin_call(const char* __plugin, const char* __arg);

static gmb_double_t gmb_user_Main(void) {
    gmb_double_t __ret = 0;

#line 3 "hello.bas"
    gmb_print_stage_begin();
    gmb_print_string(gmb_string_alloc("hello"));
    gmb_print_double(((6) * (7)));
    gmb_print_newline();
    gmb_print_stage_end(1);
    return __ret;
}

int main(int argc, char** argv) {
    gmb_set_command_args(argc, argv);
    __gmb_udt_reg_SPRITEINFO();
    gmb_user_Main();
    return 0;
}
