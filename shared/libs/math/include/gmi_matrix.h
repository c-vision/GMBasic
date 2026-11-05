/* ============================================================================
 * gmi_matrix.h — Matrice unica (algebra lineare, come MathNet nel C#).
 * Un solo registry di handle → Matrix<double>. Usata sia da MATRIX (comandi
 * grafici) sia da LINALG (funzioni algebriche): stessa entità, due front-end.
 * ============================================================================ */
#ifndef GMB_MATRIX_H
#define GMB_MATRIX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Registry ---- */
void gmi_matrix_init(void);
void gmi_matrix_shutdown(void);

/* Crea una matrice rows×cols (identità se quadrata, altrimenti zeros).
   Ritorna handle (>0) o -1. */
int64_t gmi_matrix_create(int rows, int cols);

/* Crea matrice da array di valori (row-major). Ritorna handle o -1. */
int64_t gmi_matrix_create_from(int rows, int cols, const double* data);

/* ---- Accesso ---- */
int gmi_matrix_rows(int64_t h);
int gmi_matrix_cols(int64_t h);
double gmi_matrix_get(int64_t h, int r, int c);
void gmi_matrix_set(int64_t h, int r, int c, double v);
void gmi_matrix_free(int64_t h);

/* ---- Operazioni (modificano la matrice in place o creano nuova) ---- */
int gmi_matrix_identity(int64_t h);
/* A *= B (se compatibili). Ritorna 0 ok, -1 errore. */
int gmi_matrix_multiply_into(int64_t out, int64_t a, int64_t b);
/* Crea C = A * B. Ritorna handle o -1. */
int64_t gmi_matrix_multiply(int64_t a, int64_t b);
int64_t gmi_matrix_transpose(int64_t a);
int64_t gmi_matrix_inverse(int64_t a);
double gmi_matrix_determinant(int64_t a);

/* Trasformazioni (4x4, front-end grafico) — operano su una matrice 4x4 */
int gmi_matrix_translate(int64_t h, double x, double y, double z);
int gmi_matrix_rotate_x(int64_t h, double deg);
int gmi_matrix_rotate_y(int64_t h, double deg);
int gmi_matrix_rotate_z(int64_t h, double deg);
int gmi_matrix_scale(int64_t h, double x, double y, double z);

/* ---- Vettore (stessa filosofia: un solo tipo) ---- */
int64_t gmi_vector_create(double x, double y, double z, double w);
int gmi_vector_get(int64_t h, int i, double* out);          /* 0=x,1=y,2=z,3=w */
void gmi_vector_set(int64_t h, int i, double v);
void gmi_vector_add(int64_t h, int64_t other);
void gmi_vector_sub(int64_t h, int64_t other);
void gmi_vector_mul(int64_t h, double s);
void gmi_vector_div(int64_t h, double s);
double gmi_vector_norm(int64_t h);
double gmi_vector_dot(int64_t a, int64_t b);
double gmi_vector_dot_flat(const double* a, int alen, const double* b, int blen);
double gmi_vector_norm_flat(const double* a, int alen);
/* Metriche di distanza tra vettori (VECTOR namespace) */
double gmi_vector_manhattan(int64_t a, int64_t b);
double gmi_vector_chebyshev(int64_t a, int64_t b);
double gmi_vector_cosine(int64_t a, int64_t b);
double gmi_vector_minkowski(int64_t a, int64_t b, double p);
void gmi_vector_fill_random(int64_t h, double lo, double hi);
void gmi_vector_wrap(int64_t h, double min, double max);
void gmi_vector_free(int64_t h);
/* Helper comandi VECTOR (forma handle, parità C99 con IL) */
int gmi_vector_valid(int64_t h);
double gmi_wrap_range(double v, double mn, double mx); /* semantica wrap IL (fmod) */
void gmi_vector_add_scalar(int64_t h, double s);
void gmi_vector_sub_scalar(int64_t h, double s);
void gmi_vector_mul_vec(int64_t h, int64_t other);
void gmi_vector_div_vec(int64_t h, int64_t other);
void gmi_vector_set_all(int64_t h, double val);
void gmi_vector_add_wrap_vec(int64_t h, int64_t other, double mn, double mx);
void gmi_vector_add_wrap_scalar(int64_t h, double s, double mn, double mx);
void gmi_vector_bounce2d(int64_t pos, int64_t vel, double width, double height);
void gmi_vector_bounce2d_xy(int64_t xp, int64_t yp, int64_t xv, int64_t yv,
    double xmin, double ymin, double xmax, double ymax);

/* ---- Decomposizioni (algoritmi reali) ---- */
/* Ritornano handle di decomposizione (primi due elementi: tipo, handle matrice base). */
int64_t gmi_matrix_lu(int64_t a);           /* LU con pivot parziale */
int64_t gmi_matrix_qr(int64_t a);           /* QR via Householder */
int64_t gmi_matrix_svd(int64_t a);          /* SVD via Jacobi one-sided */
int64_t gmi_matrix_evd(int64_t a);          /* EVD via Jacobi (simmetrica) */
int64_t gmi_matrix_cholesky(int64_t a);     /* Cholesky (simmetrica def. positiva) */
/* Funzioni matriciali (scipy.linalg) */
int64_t gmi_matrix_expm(int64_t a);
int64_t gmi_matrix_kron(int64_t a, int64_t b);
int64_t gmi_matrix_sqrtm(int64_t a);      /* scipy.linalg.sqrtm via EVD simmetrica */
int64_t gmi_matrix_logm(int64_t a);       /* scipy.linalg.logm via EVD simmetrica */
int64_t gmi_matrix_hessenberg(int64_t a); /* scipy.linalg.hessenberg (Householder) */
/* Completamento scipy.linalg (cosm..schur) */
int64_t gmi_matrix_cosm(int64_t a);
int64_t gmi_matrix_sinm(int64_t a);
int64_t gmi_matrix_tanm(int64_t a);
int64_t gmi_matrix_signm(int64_t a);
int64_t gmi_matrix_fractional_power(int64_t a, double p);
int64_t gmi_matrix_orth(int64_t a);
int gmi_matrix_subspace_angles(int64_t a, int64_t b, double** out);
int64_t gmi_matrix_polar(int64_t a);
double gmi_matrix_cond(int64_t a);
double gmi_matrix_nuclear_norm(int64_t a);
int64_t gmi_matrix_toeplitz(const double* c, const double* r, int n, int m);
int64_t gmi_matrix_hankel(const double* c, const double* r, int n, int m);
int64_t gmi_matrix_circulant(const double* c, int n);
int64_t gmi_matrix_solve_triangular(int64_t a, const double* b, int nb, int lower);
int64_t gmi_matrix_eig(int64_t a);
int64_t gmi_matrix_schur(int64_t a);
int64_t gmi_matrix_decomp_get_factor_named(int64_t decomp, const char* name);
double* gmi_matrix_subspace_angles_alloc(int64_t a, int64_t b, int64_t* outn);
double* gmi_matrix_row_sums_alloc(int64_t h, int64_t* outn);
double* gmi_matrix_row_means_alloc(int64_t h, int64_t* outn);
double* gmi_matrix_col_means_alloc(int64_t h, int64_t* outn);
double* gmi_matrix_col_sums_alloc(int64_t h, int64_t* outn);
double* gmi_matrix_row_variances_alloc(int64_t h, int64_t* outn);
double* gmi_matrix_col_variances_alloc(int64_t h, int64_t* outn);
double* gmi_matrix_argmax_alloc(int64_t h, int64_t* outn);
double* gmi_matrix_argmin_alloc(int64_t h, int64_t* outn);
double gmi_matrix_frobenius_norm(int64_t h);
double gmi_matrix_infinity_norm(int64_t h);
double gmi_matrix_one_norm(int64_t h);
double gmi_matrix_max_value(int64_t h);
double gmi_matrix_min_value(int64_t h);
int gmi_matrix_is_lower_triangular(int64_t h);
int gmi_matrix_is_upper_triangular(int64_t h);
int gmi_matrix_is_symmetric(int64_t h);
int gmi_matrix_is_orthogonal(int64_t h);
int64_t gmi_matrix_clone(int64_t h);
int64_t gmi_matrix_reshape(int64_t h, int new_rows, int new_cols);
int64_t gmi_matrix_concat_rows(int64_t a, int64_t b);
int64_t gmi_matrix_concat_cols(int64_t a, int64_t b);
bool* gmi_matrix_above_mask_alloc(int64_t h, double threshold, int64_t* outn);
bool* gmi_matrix_above_matrix_alloc(int64_t h, double threshold, int64_t* outn);
int64_t gmi_matrix_add_row_vector(int64_t h, const double* v, int vlen);
int64_t gmi_matrix_add_col_vector(int64_t h, const double* v, int vlen);
double* gmi_matrix_multiply_vector_alloc(int64_t h, const double* v, int vlen, int64_t* outn);
double* gmi_matrix_multiply_left_vector_alloc(const double* v, int vlen, int64_t h, int64_t* outn);
double* gmi_matrix_slice_alloc(int64_t h, int r0, int r1, int c0, int c1, int64_t* outn);
char* gmi_matrix_as_csv(int64_t h);
char* gmi_matrix_as_json(int64_t h);
int64_t gmi_matrix_load_csv(const char* path);
int64_t gmi_matrix_load_json(const char* path);
int64_t gmi_matrix_slice(int64_t h, int r0, int r1, int c0, int c1);


double* gmi_matrix_solve_triangular_alloc(int64_t a, const double* b, int nb, int lower, int64_t* outn);
double* gmi_matrix_solve_alloc(int64_t a, const double* b, int nb, int64_t* outn);

/* GetFactor(decomp, idx): idx 0=L,1=U (LU); 0=Q,1=R (QR); 0=U,1=S,2=V (SVD);
   0=Valori,1=Vettori (EVD); 0=L (Cholesky). Ritorna handle matrice o -1. */
int64_t gmi_matrix_decomp_get_factor(int64_t decomp, int idx);
void gmi_matrix_decomp_free(int64_t decomp);

/* ---- Linalg aggiuntive (port CommonLib/Numerics/LinAlg.cs) ----
   Convenzione ritorno: matrice → handle int64_t (nuova matrice); scalare → double;
   rank → int. I risultati "vettore" (LuSolve/Lstsq) sono matrici N×1. */
int gmi_matrix_rank(int64_t a);
double gmi_matrix_trace(int64_t a);
double gmi_matrix_norm(int64_t a);
int64_t gmi_matrix_pseudo_inverse(int64_t a);
int64_t gmi_matrix_null_space(int64_t a);
int64_t gmi_matrix_lu_solve(int64_t a, int64_t b);
int64_t gmi_matrix_kronecker(int64_t a, int64_t b);
int64_t gmi_matrix_block_diag(const int64_t* handles, int count);
int64_t gmi_matrix_diag_of_product(int64_t a, int64_t b);

#ifdef __cplusplus
}
#endif

#endif /* GMB_MATRIX_H */

int64_t gmi_matrix_from_flat(const double* flat, int rows, int cols);
