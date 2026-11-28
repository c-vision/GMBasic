/* gmb_c99_emit_audio.c — emissione del dominio AUDIO (GMB_STMT_AUDIO) per C99Native.
 *
 * Prima di questo file la leg C99 era MUTA sull'audio: il parser nativo riduceva
 * SOUND/PLAY/FILTER a GMB_STMT_REM, quindi non c'era nulla da emettere e un programma
 * che suonava compilava senza suono né errore. Ora i tre comandi producono chiamate al
 * runtime audio condiviso (gmb_runtime_c):
 *
 *   SOUND  [ON voice,] freq, dur[, vol[, wave[, att,dec,sus,rel[, pw]]]]
 *          → gmb_audio_sound_voice(...)   canale stabile = voice, 6 forme, ADSR, pw,
 *                                         filtro del voice
 *   PLAY   "mml" [ON canale] [TIMES n] | CONCURRENT a, b, …
 *          → gmb_audio_play_mml(...)      una sequenza per volta (la leg C non ha code
 *                                         nominate né un sequencer multi-voce: CONCURRENT
 *                                         e QUEUE suonano in sequenza, come la VM/C)
 *   FILTER voice, type, cutoff, resonance
 *          → gmb_audio_set_filter(...)
 *   PLAY PAUSE|RESUME|STOP [canale|"ALL"]
 *          → gmb_audio_control(azione, canale)
 *
 * I buchi degli argomenti opzionali si riempiono con i default di
 * AudioManager.SoundOnVoiceAsync (voice 0, vol 100, wave 0, A 10%, D 0, S 100%, R 20%,
 * pw 25%): SOUND 440, 0.5 suona come sulle altre gambe.
 */
#include "gmb_c99_emit_internal.h"

/* Un argomento numerico: l'espressione, oppure il default quando lo statement non lo porta. */
static void a_num(ctx_t* c, gmb_expr_t* e, const char* def)
{
    if (e) emit_expr(c, e);
    else sb_put(&c->out, def);
}

/* SOUND: dieci parametri posizionali, ognuno con il suo default (vedi intestazione). */
static void emit_audio_sound(ctx_t* c, gmb_stmt_t* s)
{
    static const char* def[10] = { "0", "0", "0", "100", "0", "0.10", "0.0", "1.0", "0.20", "0.25" };
    ind(c);
    sb_put(&c->out, "gmb_audio_sound_voice(");
    for (int i = 0; i < 10; i++)
    {
        if (i) sb_put(&c->out, ", ");
        a_num(c, (i < s->n_netargs) ? s->net_args[i] : NULL, def[i]);
    }
    sb_put(&c->out, ");\n");
}

/* PLAY: una chiamata per sequenza; con TIMES n un ciclo di n ripetizioni. */
static void emit_audio_play(ctx_t* c, gmb_stmt_t* s)
{
    int have_first = (s->n_netargs > 0 && s->net_args[0]);
    if (s->go_wait && have_first)
    {
        ind(c);
        sb_put(&c->out, "{ long _gmb_rep = (long)(");
        emit_expr(c, s->go_wait);
        sb_put(&c->out, ");\n");
        ind(c);
        sb_put(&c->out, "  for (long _gmb_i = 0; _gmb_i < _gmb_rep; _gmb_i++) gmb_audio_play_mml((");
        emit_expr(c, s->net_args[0]);
        sb_put(&c->out, ").data); }\n");   /* gmb_string_t -> const char* */
        return;
    }
    for (int i = 0; i < s->n_netargs; i++)
    {
        if (!s->net_args[i]) continue;
        ind(c);
        sb_put(&c->out, "gmb_audio_play_mml((");   /* gmb_string_t -> const char* (.data) */
        emit_expr(c, s->net_args[i]);
        sb_put(&c->out, ").data);\n");
    }
}

/* FILTER voice, type, cutoff, resonance. */
static void emit_audio_filter(ctx_t* c, gmb_stmt_t* s)
{
    static const char* def[4] = { "0", "0", "1000", "0" };
    ind(c);
    sb_put(&c->out, "gmb_audio_set_filter(");
    for (int i = 0; i < 4; i++)
    {
        if (i) sb_put(&c->out, ", ");
        a_num(c, (i < s->n_netargs) ? s->net_args[i] : NULL, def[i]);
    }
    sb_put(&c->out, ");\n");
}

/* PLAY PAUSE|RESUME|STOP [canale|"ALL"]: azione in s->flag, canale −1 = tutti. */
static void emit_audio_control(ctx_t* c, gmb_stmt_t* s)
{
    gmb_expr_t* ch = (s->n_netargs > 0) ? s->net_args[0] : NULL;
    ind(c);
    sb_printf(&c->out, "gmb_audio_control(%d, ", s->flag);
    if (ch && ch->kind != GMB_EXPR_STR) emit_expr(c, ch);   /* "ALL" → -1 */
    else sb_put(&c->out, "-1");
    sb_put(&c->out, ");\n");
}

void emit_audio_domain(ctx_t* c, gmb_stmt_t* s)
{
    switch (s->net_sub)
    {
        case 0: emit_audio_sound(c, s); break;
        case 1: emit_audio_play(c, s); break;
        case 2: emit_audio_filter(c, s); break;
        case 3: emit_audio_control(c, s); break;
        default: c99_fail(c, s->line, "sottocomando audio sconosciuto (%d)", s->net_sub); break;
    }
}
