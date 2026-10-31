/* native/repl/ml.h — speculare a CliMl.cs: gestione librerie ML (~/.gmbasic/libs/ml/<rid>). */
#ifndef GMB_ML_H
#define GMB_ML_H

#include <stdbool.h>

int gmb_ml_status(void);
int gmb_ml_rm(void);
int gmb_ml_setup(bool force);

#endif
