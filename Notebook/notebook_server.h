/* Notebook/notebook_server.h — server HTTP del notebook (127.0.0.1, JSON). */
#ifndef GMB_NOTEBOOK_SERVER_H
#define GMB_NOTEBOOK_SERVER_H

/* Avvia il server e blocca il chiamante fino a /shutdown (o SIGINT/SIGTERM).
 * `autosave` (può essere NULL): a ogni cella eseguita scrive lì lo snapshot delle
 * variabili in JSON. Ritorna 0 se il server è partito e si è chiuso normalmente. */
int nb_server_run(int port, const char* autosave);

#endif /* GMB_NOTEBOOK_SERVER_H */
