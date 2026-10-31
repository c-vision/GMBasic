/* gmb_job_registry.h — registro job training/apply (R2.4).
 *
 * Ring buffer degli ultimi N job finetune/apply, ciascuno con un ID
 * univoco auto-incrementante. Il registro permette di correlare una
 * lettura (AI.FINETUNELOSS etc.) a uno specifico job storico.
 *
 * Uso: gmb_job_registry_add() dopo ogni IPC response, poi BASIC
 * puo' chiamare gmb_job_registry_get() per ID o l'ultimo.
 */
#ifndef GMB_JOB_REGISTRY_H
#define GMB_JOB_REGISTRY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_JOB_REGISTRY_CAP 64

typedef enum {
    GMB_JOB_FINETUNE,
    GMB_JOB_LORA_APPLY,
} gmb_job_kind;

typedef enum {
    GMB_JOB_STATUS_OK,
    GMB_JOB_STATUS_ERROR,
    GMB_JOB_STATUS_RUNNING,
} gmb_job_status;

typedef struct {
    int             id;          /* univoco, incrementale */
    gmb_job_kind    kind;        /* finetune o apply */
    gmb_job_status  status;      /* ok / error / running */
    double          train_loss;  /* -1 se non disponibile */
    double          hold_loss;
    double          base_train;
    double          base_hold;
    long            greedy_before;
    long            greedy_after;
    long            demo_changed;
    char            ckpt[256];   /* prefisso checkpoint */
    char            message[512]; /* status o errore testuale */
} gmb_job_record;

/* Aggiunge un job completato al registro.
 * kind, status, messaggio e metriche.
 * Ritorna il job ID assegnato (>= 1). */
int gmb_job_registry_add(gmb_job_kind kind, gmb_job_status status,
                         const char* message,
                         double train_loss, double hold_loss,
                         double base_train, double base_hold,
                         long greedy_before, long greedy_after,
                         long demo_changed, const char* ckpt);

/* Ritorna l'ultimo job ID (0 se nessun job). */
int gmb_job_registry_last_id(void);

/* Ritorna il record per un dato ID (NULL se non trovato). */
const gmb_job_record* gmb_job_registry_get(int id);

/* Conta i job nel registro (fino a GMB_JOB_REGISTRY_CAP). */
int gmb_job_registry_count(void);

/* Ritorna il record all'indice [0..count-1] in ordine di inserimento (NULL se fuori range). */
const gmb_job_record* gmb_job_registry_at(int index);

/* Pulisce il registro. */
void gmb_job_registry_clear(void);

/* Rimuove la run `id` (shift); ritorna 1 se trovata/rimossa, 0 altrimenti (TRAIN STOP). */
int gmb_job_registry_remove(int id);

#ifdef __cplusplus
}
#endif

#endif /* GMB_JOB_REGISTRY_H */