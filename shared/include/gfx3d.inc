' gfx3d.inc — namespace GFX3D via FFI (libgmbgraphics: gmbsuite/gmbgraphics).
' Vettori [x,y,z], matrici 4x4 (16 valori, column-major), AABB [min3,max3], frustum 6 piani
' [a,b,c,d], raggi [ox,oy,oz,dx,dy,dz], triangoli 9 valori. Mesh/raster = handle (LONG).
' Convenzione: ritorno 1 = colpito/dentro; le scritture vanno in out() (0 = ok, -1 = errore).
NAMESPACE GFX3D

' ── geometria ──────────────────────────────────────────────────────────────
' Proiezione prospettica: m() riceve 16 valori.
DECLARE FUNCTION PERSPECTIVE LIB "gmbgraphics" ALIAS "ofx_gfx_perspective" (fovy AS DOUBLE, aspect AS DOUBLE, znear AS DOUBLE, zfar AS DOUBLE, m() AS DOUBLE) AS LONG
' Proiezione ortografica.
DECLARE FUNCTION ORTHO LIB "gmbgraphics" ALIAS "ofx_gfx_ortho" (left AS DOUBLE, right AS DOUBLE, bottom AS DOUBLE, top AS DOUBLE, znear AS DOUBLE, zfar AS DOUBLE, m() AS DOUBLE) AS LONG
' Matrice di vista look-at.
DECLARE FUNCTION LOOKAT LIB "gmbgraphics" ALIAS "ofx_gfx_lookat" (eye() AS DOUBLE, center() AS DOUBLE, up() AS DOUBLE, m() AS DOUBLE) AS LONG
' AABB dai punti (array piatto x,y,z,…; n = numero di VALORI, multiplo di 3).
DECLARE FUNCTION AABBFROMPOINTS LIB "gmbgraphics" ALIAS "ofx_gfx_aabb_from_points" (pts() AS DOUBLE, n AS LONG, out() AS DOUBLE) AS LONG
' Unione di due AABB.
DECLARE FUNCTION AABBMERGE LIB "gmbgraphics" ALIAS "ofx_gfx_aabb_merge" (a() AS DOUBLE, b() AS DOUBLE, out() AS DOUBLE) AS LONG
' Centro / estensioni di un AABB.
DECLARE FUNCTION AABBCENTER LIB "gmbgraphics" ALIAS "ofx_gfx_aabb_center" (a() AS DOUBLE, out() AS DOUBLE) AS LONG
DECLARE FUNCTION AABBEXTENTS LIB "gmbgraphics" ALIAS "ofx_gfx_aabb_extents" (a() AS DOUBLE, out() AS DOUBLE) AS LONG
' Test punto in AABB / intersezione AABB.
DECLARE FUNCTION AABBTESTPOINT LIB "gmbgraphics" ALIAS "ofx_gfx_aabb_test_point" (a() AS DOUBLE, p() AS DOUBLE) AS LONG
DECLARE FUNCTION AABBINTERSECT LIB "gmbgraphics" ALIAS "ofx_gfx_aabb_intersect" (a() AS DOUBLE, b() AS DOUBLE) AS LONG
' Frustum da matrice (6 piani = 24 valori) e test punto/AABB.
DECLARE FUNCTION FRUSTUMFROM LIB "gmbgraphics" ALIAS "ofx_gfx_frustum_from" (m() AS DOUBLE, planes() AS DOUBLE) AS LONG
DECLARE FUNCTION FRUSTUMTESTPOINT LIB "gmbgraphics" ALIAS "ofx_gfx_frustum_test_point" (planes() AS DOUBLE, p() AS DOUBLE) AS LONG
DECLARE FUNCTION FRUSTUMTESTAABB LIB "gmbgraphics" ALIAS "ofx_gfx_frustum_test_aabb" (planes() AS DOUBLE, box() AS DOUBLE) AS LONG
' Decomposizione SRT: m = T*R*S -> scale(), rot() (quaternione 4), pos().
DECLARE FUNCTION DECOMPOSESRT LIB "gmbgraphics" ALIAS "ofx_gfx_decompose_srt" (m() AS DOUBLE, scale() AS DOUBLE, rot() AS DOUBLE, pos() AS DOUBLE) AS LONG
' Camera: vista / proiezione / view-projection.
DECLARE FUNCTION CAMERAVIEW LIB "gmbgraphics" ALIAS "ofx_gfx_camera_view" (pos() AS DOUBLE, target() AS DOUBLE, up() AS DOUBLE, m() AS DOUBLE) AS LONG
DECLARE FUNCTION CAMERAPROJECTION LIB "gmbgraphics" ALIAS "ofx_gfx_camera_projection" (fov AS DOUBLE, aspect AS DOUBLE, znear AS DOUBLE, zfar AS DOUBLE, m() AS DOUBLE) AS LONG
DECLARE FUNCTION CAMERAVIEWPROJECTION LIB "gmbgraphics" ALIAS "ofx_gfx_camera_view_projection" (pos() AS DOUBLE, target() AS DOUBLE, up() AS DOUBLE, fov AS DOUBLE, aspect AS DOUBLE, znear AS DOUBLE, zfar AS DOUBLE, m() AS DOUBLE) AS LONG

' ── raggi e intersezioni ──────────────────────────────────────────────────
' Sfera: sphere() = [cx,cy,cz,r]; t(0) = parametro del colpo più vicino.
DECLARE FUNCTION RAYSPHERE LIB "gmbgraphics" ALIAS "ofx_gfx_ray_sphere" (ray() AS DOUBLE, sphere() AS DOUBLE, t() AS DOUBLE) AS LONG
' Piano: plane() = [px,py,pz,nx,ny,nz].
DECLARE FUNCTION RAYPLANE LIB "gmbgraphics" ALIAS "ofx_gfx_ray_plane" (ray() AS DOUBLE, plane() AS DOUBLE, t() AS DOUBLE) AS LONG
' Triangolo (Möller–Trumbore): tri() = 9 valori, tuv() = [t,u,v] baricentriche.
DECLARE FUNCTION RAYTRIANGLE LIB "gmbgraphics" ALIAS "ofx_gfx_ray_triangle" (ray() AS DOUBLE, tri() AS DOUBLE, tuv() AS DOUBLE) AS LONG
' AABB: t() = [tmin,tmax]; RAYBOX = t della prima faccia.
DECLARE FUNCTION RAYAABB LIB "gmbgraphics" ALIAS "ofx_gfx_ray_aabb" (ray() AS DOUBLE, box() AS DOUBLE, t() AS DOUBLE) AS LONG
DECLARE FUNCTION RAYBOX LIB "gmbgraphics" ALIAS "ofx_gfx_ray_box" (ray() AS DOUBLE, box() AS DOUBLE, t() AS DOUBLE) AS LONG
' Riflessione e rifrazione (eta = n1/n2; 0 = riflessione totale interna).
DECLARE FUNCTION RAYREFLECT LIB "gmbgraphics" ALIAS "ofx_gfx_ray_reflect" (v() AS DOUBLE, n() AS DOUBLE, out() AS DOUBLE) AS LONG
DECLARE FUNCTION RAYREFRACT LIB "gmbgraphics" ALIAS "ofx_gfx_ray_refract" (v() AS DOUBLE, n() AS DOUBLE, eta AS DOUBLE, out() AS DOUBLE) AS LONG
' Phong: params() = [lr,lg,lb, or,og,ob, ka,kd,ks,shininess]; out() = RGB.
DECLARE FUNCTION SHADEPHONG LIB "gmbgraphics" ALIAS "ofx_gfx_shade_phong" (p() AS DOUBLE, n() AS DOUBLE, view() AS DOUBLE, light() AS DOUBLE, params() AS DOUBLE, out() AS DOUBLE) AS LONG
' Intersezioni sfera/segmento/triangolo.
DECLARE FUNCTION SPHERESPHERE LIB "gmbgraphics" ALIAS "ofx_gfx_sphere_sphere" (c1() AS DOUBLE, r1 AS DOUBLE, c2() AS DOUBLE, r2 AS DOUBLE) AS LONG
DECLARE FUNCTION SPHEREBOX LIB "gmbgraphics" ALIAS "ofx_gfx_sphere_box" (c() AS DOUBLE, r AS DOUBLE, box() AS DOUBLE) AS LONG
DECLARE FUNCTION SPHEREPLANE LIB "gmbgraphics" ALIAS "ofx_gfx_sphere_plane" (c() AS DOUBLE, r AS DOUBLE, plane() AS DOUBLE, dist() AS DOUBLE) AS LONG
DECLARE FUNCTION SEGMENTPLANE LIB "gmbgraphics" ALIAS "ofx_gfx_segment_plane" (a() AS DOUBLE, b() AS DOUBLE, plane() AS DOUBLE, t() AS DOUBLE) AS LONG
DECLARE FUNCTION SEGMENTSPHERE LIB "gmbgraphics" ALIAS "ofx_gfx_segment_sphere" (a() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, r AS DOUBLE, t() AS DOUBLE) AS LONG
DECLARE FUNCTION SEGMENTTRIANGLE LIB "gmbgraphics" ALIAS "ofx_gfx_segment_triangle" (a() AS DOUBLE, b() AS DOUBLE, tri() AS DOUBLE, t() AS DOUBLE) AS LONG
DECLARE FUNCTION TRIANGLETRIANGLE LIB "gmbgraphics" ALIAS "ofx_gfx_triangle_triangle" (t1() AS DOUBLE, t2() AS DOUBLE) AS LONG
' Volume rendering: n campioni, sigma(n), rgb(n*3), dt(n); out() = RGB, acc() = opacità.
DECLARE FUNCTION VOLUMERENDER LIB "gmbgraphics" ALIAS "ofx_gfx_volume_render" (n AS LONG, sigma() AS DOUBLE, rgb() AS DOUBLE, dt() AS DOUBLE, out() AS DOUBLE, acc() AS DOUBLE) AS LONG

' ── mesh (handle) ─────────────────────────────────────────────────────────
DECLARE FUNCTION MESHSPHERE LIB "gmbgraphics" ALIAS "ofx_gfx_mesh_sphere" (slices AS LONG, stacks AS LONG, radius AS DOUBLE) AS LONG
DECLARE FUNCTION MESHBOX LIB "gmbgraphics" ALIAS "ofx_gfx_mesh_box" (sx AS DOUBLE, sy AS DOUBLE, sz AS DOUBLE) AS LONG
DECLARE FUNCTION MESHPLANE LIB "gmbgraphics" ALIAS "ofx_gfx_mesh_plane" (nx AS LONG, ny AS LONG, sx AS DOUBLE, sy AS DOUBLE) AS LONG
DECLARE FUNCTION MESHCYLINDER LIB "gmbgraphics" ALIAS "ofx_gfx_mesh_cylinder" (slices AS LONG, radius AS DOUBLE, height AS DOUBLE) AS LONG
' Vertici/triangoli: MESHPOS riempie nv*3 valori, MESHTRI nt*3 indici.
DECLARE FUNCTION MESHNV LIB "gmbgraphics" ALIAS "ofx_gfx_mesh_nv" (h AS LONG) AS LONG
DECLARE FUNCTION MESHNT LIB "gmbgraphics" ALIAS "ofx_gfx_mesh_nt" (h AS LONG) AS LONG
DECLARE FUNCTION MESHPOS LIB "gmbgraphics" ALIAS "ofx_gfx_mesh_pos" (h AS LONG, out() AS DOUBLE) AS LONG
DECLARE FUNCTION MESHTRI LIB "gmbgraphics" ALIAS "ofx_gfx_mesh_tri" (h AS LONG, out() AS DOUBLE) AS LONG
DECLARE FUNCTION MESHFREE LIB "gmbgraphics" ALIAS "ofx_gfx_mesh_free" (h AS LONG) AS LONG

' ── raster software (handle) ──────────────────────────────────────────────
DECLARE FUNCTION RASTERNEW LIB "gmbgraphics" ALIAS "ofx_gfx_raster_new" (w AS LONG, h AS LONG) AS LONG
DECLARE FUNCTION RASTERFREE LIB "gmbgraphics" ALIAS "ofx_gfx_raster_free" (h AS LONG) AS LONG
DECLARE FUNCTION RASTERCLEAR LIB "gmbgraphics" ALIAS "ofx_gfx_raster_clear" (h AS LONG, depth AS DOUBLE) AS LONG
DECLARE FUNCTION RASTERGET LIB "gmbgraphics" ALIAS "ofx_gfx_raster_get" (h AS LONG, x AS LONG, y AS LONG, out() AS DOUBLE) AS LONG
DECLARE FUNCTION RASTERTRI LIB "gmbgraphics" ALIAS "ofx_gfx_raster_tri" (h AS LONG, p() AS DOUBLE, c() AS DOUBLE) AS LONG
DECLARE FUNCTION RASTERCLIPNEAR LIB "gmbgraphics" ALIAS "ofx_gfx_raster_clip_near" (tri() AS DOUBLE, znear AS DOUBLE, out() AS DOUBLE) AS LONG

' ── superfici e raycasting su griglia ─────────────────────────────────────
' Mesh da campo di altezza z(nx*ny) su [0,sx]x[0,sy], altezza z*zscale (handle).
DECLARE FUNCTION MESHSURFACE LIB "gmbgraphics" ALIAS "ofx_gfx_mesh_surface" (z() AS DOUBLE, nx AS LONG, ny AS LONG, sx AS DOUBLE, sy AS DOUBLE, zscale AS DOUBLE) AS LONG
' DDA su griglia rows*cols (grid() piatto, !=0 solido). hit() = [hit,map_x,map_y,side,perp_dist,wall_x].
DECLARE FUNCTION RAYCASTDDA LIB "gmbgraphics" ALIAS "ofx_gfx_raycast_dda" (grid() AS DOUBLE, rows AS LONG, cols AS LONG, px AS DOUBLE, py AS DOUBLE, dx AS DOUBLE, dy AS DOUBLE, hit() AS DOUBLE) AS LONG

' ── cilindro/cono e trasformazione AABB ───────────────────────────────────
DECLARE FUNCTION RAYCYLINDER LIB "gmbgraphics" ALIAS "ofx_gfx_ray_cylinder" (ray() AS DOUBLE, base() AS DOUBLE, axis() AS DOUBLE, radius AS DOUBLE, height AS DOUBLE, t() AS DOUBLE) AS LONG
DECLARE FUNCTION RAYCONE LIB "gmbgraphics" ALIAS "ofx_gfx_ray_cone" (ray() AS DOUBLE, apex() AS DOUBLE, axis() AS DOUBLE, radius AS DOUBLE, height AS DOUBLE, t() AS DOUBLE) AS LONG
DECLARE FUNCTION CYLINDERNORMAL LIB "gmbgraphics" ALIAS "ofx_gfx_cylinder_normal" (base() AS DOUBLE, axis() AS DOUBLE, p() AS DOUBLE, out() AS DOUBLE) AS LONG
DECLARE FUNCTION CONENORMAL LIB "gmbgraphics" ALIAS "ofx_gfx_cone_normal" (apex() AS DOUBLE, axis() AS DOUBLE, k AS DOUBLE, p() AS DOUBLE, out() AS DOUBLE) AS LONG
DECLARE FUNCTION AABBTRANSFORM LIB "gmbgraphics" ALIAS "ofx_gfx_aabb_transform" (a() AS DOUBLE, m() AS DOUBLE, out() AS DOUBLE) AS LONG
