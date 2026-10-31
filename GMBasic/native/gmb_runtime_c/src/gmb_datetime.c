/* ============================================================================
 * gmb_datetime.c — builtin data/ora (DateUtils/TimerUtils C#).
 * Parte di gmb_runtime_c: libreria runtime C99 multi-file per BASIC transpilato.
 * La data è rappresentata come giorni-seriali da 1970-01-01 (gmb_int_t).
 * ============================================================================ */

#include "gmb_runtime.h"
#include <time.h>
#include <string.h>
#include <ctype.h>

/* --- Conversione data civile <-> giorni-seriali (Howard Hinnant's algorithm) --- */
static int gmb_dt_is_leap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

static int gmb_dt_days_in_month(int y, int m) {
    static const int dm[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2 && gmb_dt_is_leap(y)) return 29;
    return dm[m-1];
}

/* giorni da 1970-01-01 */
static gmb_int_t gmb_dt_days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (gmb_int_t)(era * 146097 + (gmb_int_t)doe - 719468);
}

static void gmb_dt_civil_from_days(gmb_int_t z, int* y, int* m, int* d) {
    z += 719468;
    int era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    int yy = (int)yoe + era * 400;
    unsigned doy = doe - (365*yoe + yoe/4 - yoe/100);
    unsigned mp = (5*doy + 2)/153;
    *d = (int)(doy - (153*mp+2)/5 + 1);
    *m = (int)(mp + (mp < 10 ? 3 : -9));
    *y = yy + (*m <= 2);
}

/* Parsing data → OADate (giorni dal 1899-12-30). Formati invariant: YYYY-MM-DD, MM/DD/YYYY. */
static gmb_double_t gmb_dt_parse_oa(const char* s) {
    if (!s) return 0;
    int a=0, b=0, c=0;
    if (sscanf(s, "%d-%d-%d", &a, &b, &c) == 3) {
        if (b < 1 || b > 12 || c < 1 || c > gmb_dt_days_in_month(a, b)) return 0;
        return (gmb_double_t)(gmb_dt_days_from_civil(a, b, c) + 25569);
    }
    if (sscanf(s, "%d/%d/%d", &a, &b, &c) == 3) {
        /* invariant: MM/DD/YYYY */
        if (a < 1 || a > 12 || b < 1 || b > gmb_dt_days_in_month(c, a)) return 0;
        return (gmb_double_t)(gmb_dt_days_from_civil(c, a, b) + 25569);
    }
    return 0;
}

/* DATEVALUE("YYYY-MM-DD") → OADate. */
gmb_double_t gmb_dt_datevalue(gmb_string_t s) {
    return gmb_dt_parse_oa(s.data ? s.data : "");
}

/* Conversione di un gmb_val_t DINAMICO (campo JSON) verso un campo/var DATE (OADate):
 * numero = seriale; stringa = parse ISO/parse-abile. Usato dagli emitter  * C99Native quando assegnano un campo DATA da un valore JSON (prima gmb_val_to_double →
 * atof("2023-01-06")=2023, non la data). Mai eccezione. */
gmb_double_t gmb_val_to_oadate(gmb_val_t v) {
    if (v.kind == GMB_TYPE_DOUBLE) return v.v_double;
    if (v.kind == GMB_TYPE_INT) return (gmb_double_t)v.v_int;
    if (v.kind == GMB_TYPE_STRING) return gmb_dt_datevalue(v.v_string);
    return 0.0;
}

/* CTOD("MM/DD/YYYY" o "MM/DD/YY") → OADate (anno 2 cifre: 00-29→2000, 30-99→1900). */
gmb_double_t gmb_dt_ctod(gmb_string_t s) {
    if (!s.data) return 0;
    int a=0, b=0, c=0;
    if (sscanf(s.data, "%d/%d/%d", &a, &b, &c) == 3) {
        int y = c;
        if (y < 100) y += (y < 30) ? 2000 : 1900;
        if (a < 1 || a > 12 || b < 1 || b > gmb_dt_days_in_month(y, a)) return 0;
        return (gmb_double_t)(gmb_dt_days_from_civil(y, a, b) + 25569);
    }
    return gmb_dt_parse_oa(s.data);
}

/* STOD("yyyyMMdd" compatto) → OADate (DateUtils.CoreStod). */
gmb_double_t gmb_dt_stod(gmb_string_t s) {
    if (!s.data) return 0;
    /* esattamente 8 cifre */
    const char* p = s.data;
    while (*p == ' ' || *p == '\t') p++;
    int y=0, mo=0, d=0, digits=0;
    const char* q = p;
    while (*q >= '0' && *q <= '9') { digits++; q++; }
    if (digits == 8) {
        y = (p[0]-'0')*1000 + (p[1]-'0')*100 + (p[2]-'0')*10 + (p[3]-'0');
        mo = (p[4]-'0')*10 + (p[5]-'0');
        d = (p[6]-'0')*10 + (p[7]-'0');
        if (mo >= 1 && mo <= 12 && d >= 1 && d <= gmb_dt_days_in_month(y, mo))
            return (gmb_double_t)(gmb_dt_days_from_civil(y, mo, d) + 25569);
    }
    /* fallback: DD/MM/YYYY o YYYY-MM-DD */
    if (sscanf(p, "%d/%d/%d", &d, &mo, &y) == 3) {
        if (mo >= 1 && mo <= 12 && d >= 1 && d <= gmb_dt_days_in_month(y, mo))
            return (gmb_double_t)(gmb_dt_days_from_civil(y, mo, d) + 25569);
    }
    return gmb_dt_parse_oa(p);
}

/* DTOC(oadate) → data nel formato di SET DATE FORMAT (default "mm-dd-yyyy", come
 * ClipperState.SetDateFormat C# / ClipperDates.Dtoc). Era ISO "yyyy-MM-dd" per un
 * allineamento presunto errato: IL/PCode producono "MM-DD-YYYY" (oracolo dtoc.bas).
 * Formattazione delegata a gmb_date_format (unica implementazione, §0bis). */
int gmb_date_format(int d, int m, int y, const char* fmt, bool century, char* out, int out_cap);
gmb_string_t gmb_dt_dtoc(gmb_double_t oa) {
    int y,m,d;
    gmb_dt_civil_from_days((gmb_int_t)floor(oa) - 25569, &y, &m, &d);
    char buf[32];
    /* DTOC standard → "mm-dd-yyyy" (US), canone IL/PCode/VM-C (minitest datetime/dtoc.bas). */
    if (!gmb_date_format(d, m, y, "mm-dd-yyyy", true, buf, sizeof(buf)))
        snprintf(buf, sizeof(buf), "%02d-%02d-%04d", m, d, y);
    return gmb_string_alloc(buf);
}

/* Variante `const char*` per il BRIDGE C# (P/Invoke): copia in un buffer thread-local,
 * nessuna allocazione (single-source: chiama gmb_dt_dtoc, vedi §0bis). */
const char* gmb_dt_dtoc_cstr(gmb_double_t oa) {
    static __thread char buf[32];
    gmb_string_t s = gmb_dt_dtoc(oa);
    snprintf(buf, sizeof(buf), "%s", s.data ? s.data : "");
    gmb_string_free(&s);
    return buf;
}

/* DTOC variante RDD → ISO "yyyy-MM-dd" (minitest dbf/rdd_cursor_dates.bas: RDD.DTOC e' ISO,
 * distinto dal DTOC standard US). */
gmb_string_t gmb_dt_dtoc_iso(gmb_double_t oa) {
    int y,m,d;
    gmb_dt_civil_from_days((gmb_int_t)floor(oa) - 25569, &y, &m, &d);
    char buf[32];
    if (!gmb_date_format(d, m, y, "yyyy-MM-dd", true, buf, sizeof(buf)))
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
    return gmb_string_alloc(buf);
}

/* DTOS(oadate) → "yyyyMMdd" (compatto). */
gmb_string_t gmb_dt_dtos(gmb_double_t oa) {
    int y,m,d;
    gmb_dt_civil_from_days((gmb_int_t)floor(oa) - 25569, &y, &m, &d);
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d%02d%02d", y, m, d);
    return gmb_string_alloc(buf);
}

/* Stampa di NOW()/TODAY(): "dd/mm/yyyy hh:mm:ss" — parità con IL/PCode
   (DateTime.ToString it-IT; gap C99 "NOW() senza prefisso"). */
gmb_string_t gmb_dt_datetime_to_string(gmb_double_t oa) {
    int y,m,d;
    gmb_dt_civil_from_days((gmb_int_t)floor(oa) - 25569, &y, &m, &d);
    long total = (long)llround((oa - floor(oa)) * 86400.0);
    int hh = (int)(total / 3600), mi = (int)((total % 3600) / 60), ss = (int)(total % 60);
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d:%02d", d, m, y, hh, mi, ss);
    return gmb_string_alloc(buf);
}

/* Canone datetime in JSON: stringa ISO-8601 "yyyy-MM-ddTHH:mm:ss" (parità IL/PCode
   NormalizeForJson -> DateTime.ToString("yyyy-MM-ddTHH:mm:ss")). */
gmb_string_t gmb_dt_datetime_iso(gmb_double_t oa) {
    int y,m,d;
    gmb_dt_civil_from_days((gmb_int_t)floor(oa) - 25569, &y, &m, &d);
    long total = (long)llround((oa - floor(oa)) * 86400.0);
    int hh = (int)(total / 3600), mi = (int)((total % 3600) / 60), ss = (int)(total % 60);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d", y, m, d, hh, mi, ss);
    return gmb_string_alloc(buf);
}

/* DOW(oadate): 1=domenica .. 7=sabato. */
gmb_int_t gmb_dt_dow(gmb_double_t oa) {
    gmb_int_t days = (gmb_int_t)floor(oa) - 25569; /* OADate → days da 1970 */
    int w = (int)(days % 7); // 0=giovedì
    if (w < 0) w += 7;
    static const int dow[] = {5,6,7,1,2,3,4}; // gio,ven,sab,dom,lun,mar,mer
    return dow[w];
}

gmb_string_t gmb_dt_cdow(gmb_double_t oa) {
    static const char* names[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    int d = gmb_dt_dow(oa);
    return gmb_string_alloc(names[d-1]);
}

gmb_string_t gmb_dt_cmonth(gmb_double_t oa) {
    static const char* names[] = {"January","February","March","April","May","June",
        "July","August","September","October","November","December"};
    int y,m,d; gmb_dt_civil_from_days((gmb_int_t)floor(oa) - 25569, &y, &m, &d);
    return gmb_string_alloc(names[m-1]);
}

/* ISOWEEKOFYEAR(oadate): settimana ISO 1-53. */
gmb_int_t gmb_dt_isoweek(gmb_double_t oa) {
    gmb_int_t days = (gmb_int_t)floor(oa) - 25569;
    int y,m,d; gmb_dt_civil_from_days(days, &y, &m, &d);
    // Lunedì della settimana corrente (ISO: lun=1..dom=7).
    int iso_dow = (int)gmb_dt_dow(oa); // 1=dom..7=sab
    int iso_monday_idx = (iso_dow + 5) % 7; // 0 = lunedì della settimana corrente
    gmb_int_t monday = days - iso_monday_idx;
    // Il lunedì della settimana che contiene il 4 gennaio.
    gmb_int_t jan4 = gmb_dt_days_from_civil(y, 1, 4);
    int jan4_dow = (int)((gmb_dt_dow((gmb_double_t)(jan4 + 25569))));
    gmb_int_t jan4_monday = jan4 - ((jan4_dow + 5) % 7);
    gmb_int_t week = (monday - jan4_monday) / 7 + 1;
    if (week < 1) {
        int py = y - 1;
        gmb_int_t pjan4 = gmb_dt_days_from_civil(py, 1, 4);
        int pdow = (int)(gmb_dt_dow((gmb_double_t)(pjan4 + 25569)));
        gmb_int_t pmonday = pjan4 - ((pdow + 5) % 7);
        week = (monday - pmonday) / 7 + 1;
    }
    return week;
}

/* SECONDS() — secondi dall'ultima mezzanotte (ora locale). */
gmb_int_t gmb_dt_seconds(void) {
    time_t now = time(NULL);
    struct tm tmv;
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    return (gmb_int_t)(tmv.tm_hour * 3600 + tmv.tm_min * 60 + tmv.tm_sec);
}

/* STRTIME()/STRTI()/TIME$/TI$ — ora corrente UTC "HH:mm:ss" (canone data/ora). */
gmb_string_t gmb_time_string(void) {
    time_t now = time(NULL);
    struct tm tmv;
#if defined(_WIN32) || defined(_WIN64)
    gmtime_s(&tmv, &now);
#else
    gmtime_r(&now, &tmv);
#endif
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return gmb_string_alloc(buf);
}

/* STRDATE()/DATE$ — data corrente UTC "dd/MM/yyyy" (canone data/ora). */
gmb_string_t gmb_dt_date_string(void) {
    time_t now = time(NULL);
    struct tm tmv;
#if defined(_WIN32) || defined(_WIN64)
    gmtime_s(&tmv, &now);
#else
    gmtime_r(&now, &tmv);
#endif
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d", tmv.tm_mday, tmv.tm_mon + 1, tmv.tm_year + 1900);
    return gmb_string_alloc(buf);
}

/* DATE() — seriale del solo giorno corrente UTC (canone data/ora; TIME() = gmb_dt_now()). */
gmb_double_t gmb_dt_dayserial(void) {
    time_t now = time(NULL);
    return (gmb_double_t)(now / 86400) + 25569.0;
}

/* --- OADate utils per la VM (giorni dal 1899-12-30, frazione = ora) --- */

/* NOW()/TODAY(): OADate corrente (double). */
gmb_double_t gmb_dt_now(void) {
    time_t now = time(NULL);
    return (gmb_double_t)now / 86400.0 + 25569.0;
}
gmb_double_t gmb_dt_today(void) {
    time_t now = time(NULL);
    struct tm lt;
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&lt, &now);
#else
    localtime_r(&now, &lt);
#endif
    /* OADate dalle COMPONENTI LOCALI (anno/mese/giorno). `midnight/86400 + 25569` sbagliava
     * nei fusi non-UTC: l'epoch della mezzanotte locale non è un multiplo di 86400, quindi
     * TODAY() usciva spostato (misurato su UTC+2: 46275.9166666667 invece di 46276 →
     * divergenza col backend IL che stampa 46276). */
    return gmb_dt_dateserial(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
}
/* TIME_FUNC(): frazione di giorno corrente. */
gmb_double_t gmb_dt_timeofday(void) {
    time_t now = time(NULL);
    struct tm lt;
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&lt, &now);
#else
    localtime_r(&now, &lt);
#endif
    return (lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec) / 86400.0;
}
/* ISDATE(str): 1 se data "YYYY-MM-DD" o "DD/MM/YYYY" valida. */
gmb_int_t gmb_dt_isdate(const char* s) {
    if (!s || !*s) return 0;
    int a=0,b=0,c=0;
    if (sscanf(s, "%d-%d-%d", &a, &b, &c) == 3) {
        if (b >= 1 && b <= 12 && c >= 1) {
            static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
            int maxd = mdays[b-1] + (b == 2 && gmb_dt_is_leap(a) ? 1 : 0);
            if (c <= maxd) return 1;
        }
    }
    if (sscanf(s, "%d/%d/%d", &a, &b, &c) == 3) {
        /* DD/MM/YYYY (parità IL, che accetta il formato italiano): a=giorno, b=mese, c=anno.
         * Prima era validato come MM/DD/YYYY (a<=12) → "15/01/2024" risultava non-data. */
        if (b >= 1 && b <= 12 && a >= 1) {
            static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
            int maxd = mdays[b-1] + (b == 2 && gmb_dt_is_leap(c) ? 1 : 0);
            if (a <= maxd) return 1;
        }
    }
    return 0;
}
/* EOMONTH(oadate, months): fine mese in OADate. */
gmb_double_t gmb_dt_eomonth(gmb_double_t oa, int months) {
    int y, m, d;
    long days = (long)floor(oa) - 25569; /* OADate → giorni seriali da 1970 */
    gmb_dt_civil_from_days(days, &y, &m, &d);
    int total = (y * 12 + (m - 1)) + months;
    y = total / 12;
    m = total % 12 + 1;
    static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int maxd = mdays[m-1] + (m == 2 && gmb_dt_is_leap(y) ? 1 : 0);
    return (gmb_double_t)(gmb_dt_days_from_civil(y, m, maxd) + 25569);
}
/* WEEKDAY(oadate): 1=dom..7=sab. */
gmb_int_t gmb_dt_weekday(gmb_double_t oa) {
    long days = (long)floor(oa) - 25569;
    int w = (int)((days + 4) % 7); if (w < 0) w += 7; /* 1970-01-01 gio = 4 */
    return w + 1; /* dom=1..sab=7 */
}
/* DATEDIFF(interval, oa1, oa2): differenza in unità. */
gmb_double_t gmb_dt_datediff(const char* interval, gmb_double_t oa1, gmb_double_t oa2) {
    if (!interval || !interval[0]) return oa2 - oa1;
    switch (tolower((unsigned char)interval[0])) {
        case 'd': return oa2 - oa1;
        case 'h': return (oa2 - oa1) * 24.0;
        case 'n': return (oa2 - oa1) * 1440.0;
        case 's': return (oa2 - oa1) * 86400.0;
        case 'm': {
            int y1,m1,d1,y2,m2,d2;
            long s1 = (long)floor(oa1) - 25569, s2 = (long)floor(oa2) - 25569;
            gmb_dt_civil_from_days(s1, &y1, &m1, &d1);
            gmb_dt_civil_from_days(s2, &y2, &m2, &d2);
            return (gmb_double_t)((y2 * 12 + (m2 - 1)) - (y1 * 12 + (m1 - 1)));
        }
        case 'y': {
            int y1,m1,d1,y2,m2,d2;
            long s1 = (long)floor(oa1) - 25569, s2 = (long)floor(oa2) - 25569;
            gmb_dt_civil_from_days(s1, &y1, &m1, &d1);
            gmb_dt_civil_from_days(s2, &y2, &m2, &d2);
            return (gmb_double_t)(y2 - y1);
        }
        default: return oa2 - oa1;
    }
}
/* DATEADD(interval, oa, amount): aggiunge unità. */
gmb_double_t gmb_dt_dateadd(const char* interval, gmb_double_t oa, gmb_double_t amount) {
    if (!interval || !interval[0]) return oa;
    switch (tolower((unsigned char)interval[0])) {
        case 'd': return oa + amount;
        case 'h': return oa + amount / 24.0;
        case 'n': return oa + amount / 1440.0;
        case 's': return oa + amount / 86400.0;
        case 'm': {
            int y,m,d;
            long days = (long)floor(oa) - 25569;
            gmb_dt_civil_from_days(days, &y, &m, &d);
            int total = (y * 12 + (m - 1)) + (int)amount;
            y = total / 12; m = total % 12 + 1;
            static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
            int maxd = mdays[m-1] + (m == 2 && gmb_dt_is_leap(y) ? 1 : 0);
            if (d > maxd) d = maxd;
            return (gmb_double_t)(gmb_dt_days_from_civil(y, m, d) + 25569);
        }
        case 'y': {
            int y,m,d;
            long days = (long)floor(oa) - 25569;
            gmb_dt_civil_from_days(days, &y, &m, &d);
            y += (int)amount;
            static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
            int maxd = mdays[m-1] + (m == 2 && gmb_dt_is_leap(y) ? 1 : 0);
            if (d > maxd) d = maxd;
            return (gmb_double_t)(gmb_dt_days_from_civil(y, m, d) + 25569);
        }
        default: return oa;
    }
}

/* DATESERIAL(y, m, d) → OADate. */
gmb_double_t gmb_dt_dateserial(int y, int m, int d) {
    if (m < 1 || m > 12 || d < 1 || d > gmb_dt_days_in_month(y, m)) return 0;
    return (gmb_double_t)(gmb_dt_days_from_civil(y, m, d) + 25569);
}

/* YEAR(oadate) / MONTH / DAY: componenti della data. */
gmb_int_t gmb_dt_year(gmb_double_t oa) {
    int y,m,d; gmb_dt_civil_from_days((gmb_int_t)floor(oa) - 25569, &y, &m, &d);
    return y;
}
gmb_int_t gmb_dt_month(gmb_double_t oa) {
    int y,m,d; gmb_dt_civil_from_days((gmb_int_t)floor(oa) - 25569, &y, &m, &d);
    return m;
}
gmb_int_t gmb_dt_day(gmb_double_t oa) {
    int y,m,d; gmb_dt_civil_from_days((gmb_int_t)floor(oa) - 25569, &y, &m, &d);
    return d;
}

/* HOUR / MINUTE / SECOND: componenti dell'ora da un OADate. */
gmb_int_t gmb_dt_hour(gmb_double_t oa) {
    double frac = oa - floor(oa);
    return (gmb_int_t)(frac * 24.0) % 24;
}
gmb_int_t gmb_dt_minute(gmb_double_t oa) {
    double frac = oa - floor(oa);
    return (gmb_int_t)(frac * 1440.0) % 60;
}
gmb_int_t gmb_dt_second(gmb_double_t oa) {
    double frac = oa - floor(oa);
    return (gmb_int_t)(frac * 86400.0) % 60;
}

/* TIMESERIAL(h, m, s) → frazione di giorno. */
gmb_double_t gmb_dt_timeserial(int h, int mi, int s) {
    return (h * 3600.0 + mi * 60.0 + s) / 86400.0;
}

/* DAYSINMONTH(oadate): giorni nel mese. */
gmb_int_t gmb_dt_daysinmonth(gmb_double_t oa) {
    int y = gmb_dt_year(oa);
    int m = gmb_dt_month(oa);
    return gmb_dt_days_in_month(y, m);
}

/* ISLEAPYEAR(oadate): 1 se l'anno è bisestile. */
gmb_int_t gmb_dt_isleapyear(gmb_double_t oa) {
    return gmb_dt_is_leap(gmb_dt_year(oa)) ? 1 : 0;
}

/* FIRSTDAYOFMONTH(oadate): OADate del primo giorno del mese. */
gmb_double_t gmb_dt_firstdayofmonth(gmb_double_t oa) {
    int y = gmb_dt_year(oa);
    int m = gmb_dt_month(oa);
    return (gmb_double_t)(gmb_dt_days_from_civil(y, m, 1) + 25569);
}

/* DATEFROMUNIX(sec) → OADate. */
gmb_double_t gmb_dt_datefromunix(gmb_double_t unix_sec) {
    return unix_sec / 86400.0 + 25569.0;
}

/* UNIXTIMESTAMP([oadate]) → secondi Unix (UTC); senza arg → adesso. */
gmb_double_t gmb_dt_unix(gmb_double_t oa, int have_oa) {
    if (!have_oa) return (gmb_double_t)time(NULL);
    return (oa - 25569.0) * 86400.0;
}

/* ISOWEEKYEAR(oadate): anno ISO (settimana 1). */
gmb_int_t gmb_dt_isowk_year(gmb_double_t oa) {
    int w = gmb_dt_isoweek(oa);
    int y = gmb_dt_year(oa);
    /* se settimana 52/53 e gennaio è a fine anno → anno+1; se settimana 1 e dicembre → anno-1 */
    if (w >= 52 && gmb_dt_month(oa) == 1) return y - 1;
    if (w == 1 && gmb_dt_month(oa) == 12) return y + 1;
    return y;
}

/* TIMEDOLLAR(oadate) → "HH:mm:ss". */
gmb_string_t gmb_dt_timedollar(gmb_double_t oa) {
    int h = gmb_dt_hour(oa);
    int m = gmb_dt_minute(oa);
    int s = gmb_dt_second(oa);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
    return gmb_string_alloc(buf);
}

/* FIRSTDAYOFMONTH(serial) — primo giorno del mese (OADate), parity IL/VM. */
gmb_double_t gmb_firstdayofmonth(gmb_double_t oa) {
    return gmb_dt_dateserial(gmb_dt_year(oa), gmb_dt_month(oa), 1);
}
