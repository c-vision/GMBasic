/* gmb_observation.h — Registry ContextObject (todo AI-Runtime consolidato, asse A/P1).
 * Unica fonte della semantica dei testi per il mondo C; il lato C# (ObservationRegistry.cs)
 * replica gli STESSI testi (parità ancorata dai MiniTest con EXPECT). §0bis: i backend
 * chiamano queste funzioni, mai logica inline.
 * JSON canonico (chiavi in quest'ordine, nessun timestamp — determinismo):
 *   {"id":N,"type":"T","source":"S","artifact":"A","hash":"H","size":K}
 *   sconosciuto: {"error":"unknown observation"}   lista: "N|T|H" uniti da \n
 *   latest vuoto: ""   hash/file sconosciuti: ""
 * Hash = FNV-1a 64 bit, 16 cifre esadecimali minuscole (implementazione identica in C#). */
#ifndef GMB_OBSERVATION_H
#define GMB_OBSERVATION_H

#include "gmb_runtime.h"

/* Azioni (dietro il comando AI OBSERVE). Ritorno: id osservazione (>0) o -1. */
int gmb_obs_record_file(const char* path);
/* tail_lines <= 0 => default condiviso 200 righe finali. */
int gmb_obs_record_log(const char* path, int tail_lines);
/* A5: ContextObject con TYPE esplicito (fonti TEST/DATABASE/PROCESS/BROWSER). -1 se l'artefatto
 * non è leggibile (il comando alza un errore esplicito a monte). Speculare al C# RecordTyped. */
int gmb_obs_record_typed(const char* type, const char* source, const char* artifact_path);

/* A5 — fonti OBSERVE di fase 2 (gmb_observe_test.c / gmb_observe_sources.c). Ritornano l'id
 * dell'osservazione oppure -1 con l'errore ESPLICITO lasciato al chiamante (mai silenzioso). */
double       gmb_observe_test(const char* path);
double       gmb_observe_database(const char* path, const char* query);
double       gmb_observe_process(const char* name);
double       gmb_observe_browser(const char* url);

/* Letture (funzioni BASIC AI.OBSERVATION*). Stringhe sempre di proprietà del chiamante
 * (gmb_string_free). count/latest valgono anche con registry vuoto (0 / ""). */
gmb_string_t gmb_obs_latest_id(void);
gmb_string_t gmb_obs_json(int id);
gmb_string_t gmb_obs_hash(int id);
int gmb_obs_count(void);
gmb_string_t gmb_obs_list(void);
gmb_string_t gmb_obs_file(int id);

#endif
