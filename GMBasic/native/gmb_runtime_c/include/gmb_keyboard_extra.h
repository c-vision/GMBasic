/* gmb_keyboard_extra.h — parità common/CommonLib/Keyboard/{KeyCombination,KeyboardTypes,
 * ConsoleKeyboardService,HeadlessKeyboardService}.cs — flusso autonomo C (no bridge C#)
 */
#ifndef GMB_KEYBOARD_EXTRA_H
#define GMB_KEYBOARD_EXTRA_H

#include <stdint.h>

#define GMB_KEY_ENTER 13
#define GMB_KEY_BACKSPACE 8
#define GMB_KEY_TAB 9
#define GMB_KEY_ESC 27
#define GMB_KEY_LEFT 0x1B4C    /* '\x1b'+'L' encoded (vedi gmb_tui_keys) */
#define GMB_KEY_RIGHT 0x1B52
#define GMB_KEY_UP 0x1B55
#define GMB_KEY_DOWN 0x1B44
#define GMB_KEY_HOME 0x1B48
#define GMB_KEY_END 0x1B45
#define GMB_KEY_DELETE 0x1B44 + 1

#define GMB_MOD_SHIFT 1
#define GMB_MOD_ALT 2
#define GMB_MOD_CTRL 4

typedef struct gmb_key_combination {
    int key;        /* ConsoleKey (KeyCombination.cs:6) */
    int modifiers;  /* ConsoleModifiers */
} gmb_key_combination_t;

typedef struct gmb_terminal_key_info {
    char key_char;
    int key;
    int shift, alt, control;
} gmb_terminal_key_info_t;

/* --- Fase 8.1-8.5 (KeyboardServiceConsole.cs:136-833) --- */

/* 8.1 KeyCombination */
gmb_key_combination_t gmb_key_combination_new(int key, int modifiers);
int gmb_key_combination_equals(gmb_key_combination_t a, gmb_key_combination_t b);

/* 8.2/8.3 Poll/ReadKey — leggono da typeahead (gmb_console_sendkey/readkey) */
int gmb_key_try_read(int convert_uppercase);             /* non bloccante: 0 se vuoto */
int gmb_key_read_key(int timeout_ms, int convert_uppercase); /* bloccante con timeout */
int gmb_key_read_key_info(int timeout_ms, int convert_uppercase, gmb_terminal_key_info_t* out);

/* 8.4 ReadLine con editor di riga (prompt, cursore, backspace/delete/frecce/home/end) */
char* gmb_key_read_line(const char* prompt);

/* Hook ON KEY (KeyCombination -> callback), KeyboardServiceConsole.cs:637-646 */
typedef void (*gmb_key_hook_fn)(void* userdata);
void gmb_key_register_hook(gmb_key_combination_t combo, gmb_key_hook_fn fn, void* userdata);
void gmb_key_unregister_hook(gmb_key_combination_t combo);
void gmb_key_unregister_all_hooks(void);
int gmb_key_is_hook_registered(gmb_key_combination_t combo);

/* SendKeys (KeyboardServiceConsole.cs:149-161): \r|\n->Enter, \b->Backspace, altro=stampabile */
void gmb_key_send_keys(const char* keys);
void gmb_key_send_key(char key);

/* Buffer/pausa/reset (KeyboardServiceConsole.cs:649-682) */
void gmb_key_clear_buffer(void);
void gmb_key_reset(void);
void gmb_key_cancel_pending_reads(void);
void gmb_key_pause(void);
void gmb_key_resume(void);

#endif
