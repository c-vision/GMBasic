/* gmb_parser_stmt_audio.c — parser AUDIO: SOUND / PLAY / FILTER.
 *
 * Parità con Parser.Stmt.Commands.cs (ParseSoundStatement / ParsePlayStatement /
 * ParseFilterStatement). Fino al 2026-11-28 queste tre righe diventavano GMB_STMT_REM
 * ("no-op headless, 7 sample Audio"): il dominio non aveva né AST né emissione, quindi
 * la leg C99Native compilava e non emetteva NULLA (silenzio, non un errore).
 *
 * Ora producono UNO statement GMB_STMT_AUDIO, con il sottocomando in net_sub e gli
 * argomenti in net_args — lo stesso canale generico già usato da GFX/MESSAGE/SIGNAL:
 *   0 SOUND   net_args[0..9] = voice, freq, dur, vol, wave, att, dec, sus, rel, pw
 *             (NULL = argomento assente: i default li mette l'emitter, come il C#)
 *   1 PLAY    una stringa MML per voce (CONCURRENT = più voci, suonate in sequenza);
 *             per LOOP … TIMES n le ripetizioni stanno in go_wait
 *   2 FILTER  voice, type, cutoff, resonance
 *   3 PLAY PAUSE|RESUME|STOP [canale] — azione in `flag` (0 stop, 1 pausa, 2 riprendi)
 *
 * Restano fuori SOUND LOAD / SOUND PLAY #h / SOUND VOLUME / AUDIO (handle e file: nessun
 * runtime comune in C99Native): continuano a essere saltati come prima, esplicitamente
 * e non a metà riga.
 */
#include "gmb_parser_internal.h"
#include <stdlib.h>
#include <string.h>

/* Consuma fino a fine riga (i comandi audio non hanno continuazione). */
static void audio_skip_eol(gmb_parser_t* p)
{
    while (!gmb_p_at_end(p) && !gmb_p_check(p, GMB_TOK_Newline) && !gmb_p_check(p, GMB_TOK_Colon))
        gmb_p_adv(p);
}

/* Espressione facoltativa: NULL a fine riga o davanti a una virgola. */
static gmb_expr_t* audio_opt_expr(gmb_parser_t* p)
{
    const gmb_token_t* t = gmb_p_cur(p);
    if (!t || t->kind == GMB_TOK_Newline || t->kind == GMB_TOK_Colon || t->kind == GMB_TOK_Eof)
        return NULL;
    return gmb_p_parse_expr(p);
}

/* SOUND [ON voice,] freq, dur[, vol[, wave[, att, dec, sus, rel[, pw]]]] */
gmb_stmt_t* gmb_p_parse_audio_sound(gmb_parser_t* p, int line)
{
    const gmb_token_t* n1 = gmb_p_peek(p, 1);
    /* SOUND LOAD/PLAY #h/VOLUME/PAUSE/...: non è un tono. Resta il vecchio salto di riga
     * (handle e canali nominati non hanno un equivalente nel runtime C condiviso). */
    if (n1 && (n1->kind == GMB_TOK_Load || n1->kind == GMB_TOK_Play || n1->kind == GMB_TOK_Volume
               || n1->kind == GMB_TOK_Pause || n1->kind == GMB_TOK_Resume || n1->kind == GMB_TOK_Stop)) {
        gmb_p_adv(p);
        audio_skip_eol(p);
        return gmb_stmt_new(GMB_STMT_REM, line);
    }

    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_AUDIO, line);
    s->net_sub = 0;
    s->net_args = (gmb_expr_t**)calloc(10, sizeof(gmb_expr_t*));
    s->n_netargs = 10;
    if (!s->net_args) return s;

    gmb_p_adv(p);   /* SOUND */
    if (gmb_p_match(p, GMB_TOK_On)) {
        s->net_args[0] = gmb_p_parse_expr(p);   /* voice */
        gmb_p_match(p, GMB_TOK_Comma);
    }
    s->net_args[1] = gmb_p_parse_expr(p);       /* freq */
    gmb_p_match(p, GMB_TOK_Comma);
    s->net_args[2] = gmb_p_parse_expr(p);       /* dur */
    for (int k = 3; k < 10 && gmb_p_match(p, GMB_TOK_Comma); )
    {
        gmb_expr_t* e = audio_opt_expr(p);
        if (!e) break;                          /* virgola finale: niente argomento */
        s->net_args[k++] = e;
    }
    audio_skip_eol(p);
    return s;
}

/* PLAY <stringa mml> [ON canale] | CONCURRENT a, b, … | QUEUE … | LOOP … TIMES n
 *      | PAUSE|RESUME|STOP [canale|"ALL"] */
gmb_stmt_t* gmb_p_parse_audio_play(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_AUDIO, line);
    gmb_p_adv(p);   /* PLAY */

    if (gmb_p_check(p, GMB_TOK_Pause) || gmb_p_check(p, GMB_TOK_Resume) || gmb_p_check(p, GMB_TOK_Stop))
    {
        const gmb_token_t* a = gmb_p_cur(p);
        s->net_sub = 3;
        s->flag = a->kind == GMB_TOK_Pause ? 1 : (a->kind == GMB_TOK_Resume ? 2 : 0);
        gmb_p_adv(p);
        s->net_args = (gmb_expr_t**)calloc(1, sizeof(gmb_expr_t*));
        s->n_netargs = 1;
        if (s->net_args) s->net_args[0] = audio_opt_expr(p);   /* canale; NULL = tutti */
        audio_skip_eol(p);
        return s;
    }

    s->net_sub = 1;
    if (gmb_p_match(p, GMB_TOK_Concurrent))
    {
        /* PLAY CONCURRENT v1, v2, …: ogni voce è una sequenza. La leg C non ha un
         * sequencer multi-voce: l'emitter le suona in sequenza (degradazione esplicita,
         * la stessa della VM/C). */
        int cap = 4;
        s->net_args = (gmb_expr_t**)calloc((size_t)cap, sizeof(gmb_expr_t*));
        s->n_netargs = 0;
        if (!s->net_args) return s;
        for (;;)
        {
            gmb_expr_t* e = audio_opt_expr(p);
            if (!e) break;
            if (s->n_netargs >= cap) {
                cap *= 2;
                gmb_expr_t** grown = (gmb_expr_t**)realloc(s->net_args, (size_t)cap * sizeof(gmb_expr_t*));
                if (!grown) break;
                s->net_args = grown;
            }
            s->net_args[s->n_netargs++] = e;
            if (!gmb_p_match(p, GMB_TOK_Comma)) break;
        }
        if (s->n_netargs == 0)
            gmb_p_set_err(p, "PLAY CONCURRENT: attesa almeno una stringa musicale", line);
        audio_skip_eol(p);
        return s;
    }

    gmb_p_match(p, GMB_TOK_Queue);
    gmb_p_match(p, GMB_TOK_Loop);
    s->net_args = (gmb_expr_t**)calloc(1, sizeof(gmb_expr_t*));
    s->n_netargs = 1;
    if (!s->net_args) return s;
    s->net_args[0] = gmb_p_parse_expr(p);       /* sequenza MML */
    if (gmb_p_match(p, GMB_TOK_On))
    {
        /* ON canale: la leg C non ha code nominate — il canale si accetta e si ignora. */
        (void)audio_opt_expr(p);
    }
    if (gmb_p_match(p, GMB_TOK_Times))
        s->go_wait = audio_opt_expr(p);         /* LOOP … TIMES n → n ripetizioni */
    audio_skip_eol(p);
    return s;
}

/* FILTER voice, type, cutoff, resonance */
gmb_stmt_t* gmb_p_parse_audio_filter(gmb_parser_t* p, int line)
{
    gmb_stmt_t* s = gmb_stmt_new(GMB_STMT_AUDIO, line);
    s->net_sub = 2;
    s->net_args = (gmb_expr_t**)calloc(4, sizeof(gmb_expr_t*));
    s->n_netargs = 4;
    if (!s->net_args) return s;
    gmb_p_adv(p);   /* FILTER */
    s->net_args[0] = gmb_p_parse_expr(p);       /* voice */
    gmb_p_match(p, GMB_TOK_Comma);
    s->net_args[1] = gmb_p_parse_expr(p);       /* type */
    gmb_p_match(p, GMB_TOK_Comma);
    s->net_args[2] = gmb_p_parse_expr(p);       /* cutoff Hz */
    gmb_p_match(p, GMB_TOK_Comma);
    s->net_args[3] = gmb_p_parse_expr(p);       /* resonance % */
    audio_skip_eol(p);
    return s;
}
