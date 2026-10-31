/* native/repl/progress.h — speculare a ConsoleProgressBar.cs. */
#ifndef GMB_PROGRESS_H
#define GMB_PROGRESS_H

#include <stdbool.h>

void gmb_progress_init(int width);
bool gmb_progress_enabled(void);
void gmb_progress_report(int percent, long bytes_done, long bytes_total, const char* file);
void gmb_progress_finish(void);

#endif
