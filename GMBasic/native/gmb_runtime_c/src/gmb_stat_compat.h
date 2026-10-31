/* gmb_stat_compat.h — accesso PORTABILE ai campi timespec di `struct stat`.
 *
 * PERCHÉ. Il codice usava direttamente `st_mtimespec` / `st_atimespec` / `st_birthtimespec`,
 * che esistono solo su macOS: su Linux la build si fermava con
 *   "error: 'struct stat' has no member named 'st_mtimespec'".
 * Linux espone `st_mtim` / `st_atim` / `st_ctim` e NON ha il BIRTH time (creazione file).
 *
 * CONVENZIONE. Le macro prendono la VARIABILE `struct stat` (non un puntatore):
 *     struct stat st;  stat(p, &st);
 *     double t = (double)GMB_ST_MTIME_SEC(st) + (double)GMB_ST_MTIME_NSEC(st) / 1e9;
 *
 * NOTA WINDOWS. Il ramo `#elif _WIN32` mappa su `_stat64`: secondi dai campi `st_*time`,
 * nanosecondi 0 (il CRT non li espone). `st_ctime` su Windows è la CREAZIONE del file.
 */
#ifndef GMB_STAT_COMPAT_H
#define GMB_STAT_COMPAT_H

#if defined(__APPLE__)

#  define GMB_ST_MTIME_SEC(st)   ((st).st_mtimespec.tv_sec)
#  define GMB_ST_MTIME_NSEC(st)  ((st).st_mtimespec.tv_nsec)
#  define GMB_ST_ATIME_SEC(st)   ((st).st_atimespec.tv_sec)
#  define GMB_ST_ATIME_NSEC(st)  ((st).st_atimespec.tv_nsec)
#  define GMB_ST_CTIME_SEC(st)   ((st).st_ctimespec.tv_sec)
#  define GMB_ST_CTIME_NSEC(st)  ((st).st_ctimespec.tv_nsec)
#  define GMB_ST_BTIME_SEC(st)   ((st).st_birthtimespec.tv_sec)
#  define GMB_ST_BTIME_NSEC(st)  ((st).st_birthtimespec.tv_nsec)

#elif defined(_WIN32) || defined(_WIN64)
/* Windows (_stat64): NIENTE nanosecondi e nessun campo timespec. `st_ctime` su Windows è
 * la data di CREAZIONE (parità con il birth time di macOS), quindi si usa per GMB_ST_BTIME_*.
 * I *_NSEC restano 0 — non si inventa precisione che il filesystem non fornisce. */
#  define GMB_ST_MTIME_SEC(st)   ((long long)(st).st_mtime)
#  define GMB_ST_MTIME_NSEC(st)  (0)
#  define GMB_ST_ATIME_SEC(st)   ((long long)(st).st_atime)
#  define GMB_ST_ATIME_NSEC(st)  (0)
#  define GMB_ST_CTIME_SEC(st)   ((long long)(st).st_ctime)
#  define GMB_ST_CTIME_NSEC(st)  (0)
#  define GMB_ST_BTIME_SEC(st)   ((long long)(st).st_ctime)   /* creazione, come macOS */
#  define GMB_ST_BTIME_NSEC(st)  (0)

#else   /* Linux e altri POSIX: st_mtim/st_atim/st_ctim, senza birth time */

#  define GMB_ST_MTIME_SEC(st)   ((st).st_mtim.tv_sec)
#  define GMB_ST_MTIME_NSEC(st)  ((st).st_mtim.tv_nsec)
#  define GMB_ST_ATIME_SEC(st)   ((st).st_atim.tv_sec)
#  define GMB_ST_ATIME_NSEC(st)  ((st).st_atim.tv_nsec)
#  define GMB_ST_CTIME_SEC(st)   ((st).st_ctim.tv_sec)
#  define GMB_ST_CTIME_NSEC(st)  ((st).st_ctim.tv_nsec)
/* Linux non espone la data di CREAZIONE: si ripiega sul ctime (cambio metadati), che è la
 * migliore approssimazione disponibile e mantiene la semantica "0 = sconosciuto". */
#  define GMB_ST_BTIME_SEC(st)   ((st).st_ctim.tv_sec)
#  define GMB_ST_BTIME_NSEC(st)  ((st).st_ctim.tv_nsec)

#endif

#endif /* GMB_STAT_COMPAT_H */
