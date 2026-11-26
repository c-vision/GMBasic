// FILE: CommonLib.Numerics.Scene3DNative.cs
// Ponte C# → C della facciata `gmb_scene_*` (motore 3D, dominio SCENE — renderer mesh).
//
// PERCHÉ. Lo stato della scena vive SOLO nel runtime C (`gmb_3d_cmd.c`): geometria accumulata,
// camera, target, statistiche dell'ultimo RENDER. IL, PCodeVM e VM/C non ne hanno una copia e non
// devono averla — duplicare l'accumulo o il disegno significherebbe due scene divergenti
// (AGENTS §0bis: una sola sorgente). Le gambe gestite chiamano quindi le STESSE funzioni delle
// gambe C tramite la libreria condivisa `libgmb_3d` (target CMake `gmb_3d`, che compila
// `gmb_3d_cmd.c` e il kernel `gmb_3d_frame.c`).
//
// CONVENZIONE. I comandi sono COMANDI: non hanno valore di ritorno leggibile dal BASIC (§1), ma
// il CODICE di ritorno della facciata non si scarta più: `Check` lo converte in un errore BASIC
// di runtime con il messaggio canonico del C (`gmb_3d_last_error`). È la stessa regola delle
// gambe C (C99Native: `gmb_scene_checked`; VM/C: controllo attorno al dispatch SCENE), così la
// suite negativa `tools/sh/check_scene_negatives.sh` vede lo stesso testo e lo stesso exit code
// (6) su tutte e quattro le gambe. Prima un argomento fuori range era un no-op silenzioso.
//
// Le letture (SCENE.VERTS|TRIS|DRAWN|CULLED|PIXELS|HASH|PIXEL|PICK) sono funzioni e ritornano un
// numero: non hanno errori di superficie.
//
// NESSUNA LIB, NESSUN RISULTATO. Se `libgmb_3d` non è caricabile si propaga l'eccezione del
// loader (DllNotFoundException): un'alternativa gestita sarebbe uno stub, vietato (§0bis).

using System;
using System.Runtime.InteropServices;

namespace CommonLib;

public static class Scene3DNative
{
    private const string Lib = "gmb_3d";
    private const string FallbackError = "SCENE: comando fallito";

    /// <summary>
    /// Errore di superficie SCENE → eccezione BASIC di runtime. Il testo è quello del runtime C:
    /// è la stessa stringa che C99Native e VM/C mettono in `[GMBASIC] ERROR`/`[VM ERROR]`, quindi
    /// il confronto cross-leg è sul messaggio core (l'harness normalizza i prefissi).
    /// </summary>
    private static void Check(long rc)
    {
        if (rc == 0) return;
        string msg = Marshal.PtrToStringUTF8(LastError()) ?? "";
        if (msg.Length == 0) msg = FallbackError;
        Fail(msg);
    }

    /// <summary>
    /// Errore BASIC con il testo del runtime C. Si solleva <c>BasicException</c> e non
    /// un'eccezione .NET generica: è il tipo che IL e PCode convertono nel LORO meccanismo
    /// (TRY/CATCH, ON ERROR) con lo stesso testo; una generica sfuggiva al TRY di PCode.
    /// </summary>
    [System.Diagnostics.CodeAnalysis.DoesNotReturn]
    private static void Fail(string msg) =>
        throw new Basic.BasicException(Basic.ErrorCode.InvalidParameter,
                                       Basic.SourcePosition.Unknown, msg);

    [DllImport(Lib, EntryPoint = "gmb_3d_last_error", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr LastError();

    // --- comandi (rc < 0 = errore del runtime: Check lo solleva) ------------------------------
    [DllImport(Lib, EntryPoint = "gmb_scene_reset", CallingConvention = CallingConvention.Cdecl)]
    private static extern long ResetRaw();
    public static long Reset() { var rc = ResetRaw(); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_view", CallingConvention = CallingConvention.Cdecl)]
    private static extern long ViewRaw(long w, long h, double fovDeg);
    public static long View(long w, long h, double fovDeg) { var rc = ViewRaw(w, h, fovDeg); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_checker", CallingConvention = CallingConvention.Cdecl)]
    private static extern long TextureRaw(long cells);
    public static long Texture(long cells) { var rc = TextureRaw(cells); Check(rc); return rc; }

    // SCENE TEXTURE "file.png": texture da PNG (RGBA8, riga 0 in alto). Il decode e' quello
    // del runtime (`png_decode`), la texture si sostituisce solo se il file e' valido.
    [DllImport(Lib, EntryPoint = "gmb_scene_texture_file", CallingConvention = CallingConvention.Cdecl)]
    private static extern long TextureFileRaw([MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    public static long TextureFile(string path) { var rc = TextureFileRaw(path); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_object", CallingConvention = CallingConvention.Cdecl)]
    private static extern long ObjectRaw(long r, long g, long b);
    public static long Object(long r, long g, long b) { var rc = ObjectRaw(r, g, b); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_vertex", CallingConvention = CallingConvention.Cdecl)]
    private static extern long VertexRaw(double x, double y, double z, double u, double v);
    public static long Vertex(double x, double y, double z, double u, double v)
    { var rc = VertexRaw(x, y, z, u, v); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_vertex_n", CallingConvention = CallingConvention.Cdecl)]
    private static extern long VertexNRaw(double x, double y, double z, double u, double v,
                                          double nx, double ny, double nz);
    public static long VertexN(double x, double y, double z, double u, double v,
                               double nx, double ny, double nz)
    { var rc = VertexNRaw(x, y, z, u, v, nx, ny, nz); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_quad", CallingConvention = CallingConvention.Cdecl)]
    private static extern long QuadRaw(long a, long b, long c, long d);
    public static long Quad(long a, long b, long c, long d) { var rc = QuadRaw(a, b, c, d); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_box", CallingConvention = CallingConvention.Cdecl)]
    private static extern long BoxRaw(double x0, double y0, double z0, double x1, double y1, double z1);
    public static long Box(double x0, double y0, double z0, double x1, double y1, double z1)
    { var rc = BoxRaw(x0, y0, z0, x1, y1, z1); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_objpos", CallingConvention = CallingConvention.Cdecl)]
    private static extern long ObjPosRaw(double x, double y, double z, double yawDeg);
    public static long ObjPos(double x, double y, double z, double yawDeg)
    { var rc = ObjPosRaw(x, y, z, yawDeg); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_camera", CallingConvention = CallingConvention.Cdecl)]
    private static extern long CameraRaw(double x, double y, double z, double yawDeg, double pitchDeg);
    public static long Camera(double x, double y, double z, double yawDeg, double pitchDeg)
    { var rc = CameraRaw(x, y, z, yawDeg, pitchDeg); Check(rc); return rc; }

    // SCENE LIGHT dx, dy, dz, livello: luce direzionale per faccia (normale calcolata nel
    // runtime C, in aritmetica intera dopo la quantizzazione del coseno).
    [DllImport(Lib, EntryPoint = "gmb_scene_light", CallingConvention = CallingConvention.Cdecl)]
    private static extern long LightRaw(double dx, double dy, double dz, long level);
    public static long Light(double dx, double dy, double dz, long level)
    { var rc = LightRaw(dx, dy, dz, level); Check(rc); return rc; }

    // Flag di materiale: espongono bilinear/masked/two_sided già presenti nel kernel.
    [DllImport(Lib, EntryPoint = "gmb_scene_smooth", CallingConvention = CallingConvention.Cdecl)]
    private static extern long SmoothRaw(long on);
    public static long Smooth(long on) { var rc = SmoothRaw(on); Check(rc); return rc; }

    // SSAA 2x della scena (spento di default): il render disegna in un target doppio
    // e lo risolve; PIXEL/SAVE/PRESENT restano sul target logico.
    [DllImport(Lib, EntryPoint = "gmb_scene_aa", CallingConvention = CallingConvention.Cdecl)]
    private static extern long AaRaw(long on);
    public static long Aa(long on) { var rc = AaRaw(on); Check(rc); return rc; }

    // SCENE PICK on|off: buffer di id per pixel, additivo e spento di default. Con
    // il pick spento nessuna allocazione e nessuna scrittura: byte di sempre.
    [DllImport(Lib, EntryPoint = "gmb_scene_pick_enable", CallingConvention = CallingConvention.Cdecl)]
    private static extern long PickEnableRaw(long on);
    public static long PickEnable(long on) { var rc = PickEnableRaw(on); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_alpha", CallingConvention = CallingConvention.Cdecl)]
    private static extern long AlphaRaw(long on);
    public static long Alpha(long on) { var rc = AlphaRaw(on); Check(rc); return rc; }

    // ── P4: lampade, fog, FAR, cielo, colormap, thing ───────────────────────────────────────
    // Ogni voce è la facciata C omonima: il kernel resta l'unica implementazione (§0bis).
    [DllImport(Lib, EntryPoint = "gmb_scene_lamp", CallingConvention = CallingConvention.Cdecl)]
    private static extern long LampRaw(long n, double x, double y, double z, double radius, double amount);
    public static long Lamp(long n, double x, double y, double z, double radius, double amount)
    { var rc = LampRaw(n, x, y, z, radius, amount); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_lamp_spot", CallingConvention = CallingConvention.Cdecl)]
    private static extern long LampSpotRaw(long n, double x, double y, double z, double radius,
                                           double amount, double dx, double dy, double dz, double angle);
    public static long LampSpot(long n, double x, double y, double z, double radius,
                                double amount, double dx, double dy, double dz, double angle)
    { var rc = LampSpotRaw(n, x, y, z, radius, amount, dx, dy, dz, angle); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_fog", CallingConvention = CallingConvention.Cdecl)]
    private static extern long FogRaw(double d);
    public static long Fog(double d) { var rc = FogRaw(d); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_far", CallingConvention = CallingConvention.Cdecl)]
    private static extern long FarRaw(double d);
    public static long Far(double d) { var rc = FarRaw(d); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_sky", CallingConvention = CallingConvention.Cdecl)]
    private static extern long SkyRaw(double repeat, double shift);
    public static long Sky(double repeat, double shift) { var rc = SkyRaw(repeat, shift); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_colormap", CallingConvention = CallingConvention.Cdecl)]
    private static extern long ColorMapRaw();
    public static long ColorMap() { var rc = ColorMapRaw(); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_thing", CallingConvention = CallingConvention.Cdecl)]
    private static extern long ThingRaw(long n, double x, double y, double z, double w, double h);
    public static long Thing(long n, double x, double y, double z, double w, double h)
    { var rc = ThingRaw(n, x, y, z, w, h); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_object_flags", CallingConvention = CallingConvention.Cdecl)]
    private static extern long ObjectFlagsRaw(long r, long g, long b, long flags, long light);
    public static long ObjectFlags(long r, long g, long b, long flags, long light)
    { var rc = ObjectFlagsRaw(r, g, b, flags, light); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_twosided", CallingConvention = CallingConvention.Cdecl)]
    private static extern long TwoSidedRaw(long on);
    public static long TwoSided(long on) { var rc = TwoSidedRaw(on); Check(rc); return rc; }

    // SCENE PRESENT, lato 3D: guardia GPM e frame corrente. La COPIA nella pagina vive in
    // `gmb_gfx` (`gmb_gfx_present_argb`), perché le pagine 2D appartengono a quel modulo:
    // qui e lì c'è una copia ciascuno del runtime, quindi si passa il puntatore del frame.
    [DllImport(Lib, EntryPoint = "gmb_scene_present_check", CallingConvention = CallingConvention.Cdecl)]
    private static extern long PresentCheckRaw();

    [DllImport(Lib, EntryPoint = "gmb_scene_frame", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr SceneFrame(ref long w, ref long h);

    [DllImport("gmb_gfx", EntryPoint = "gmb_gfx_present_argb", CallingConvention = CallingConvention.Cdecl)]
    private static extern int GfxPresentArgb(IntPtr src, int w, int h);

    /// <summary>
    /// `SCENE PRESENT`: target 3D → pagina 2D corrente. Gira dentro il task GPM che esegue
    /// il comando (GPM = unico esecutore di BASIC, §0ter); non tocca SDL, che resta al pump
    /// del main thread. Ritorna 0 quando il frame è stato copiato, negativo altrimenti.
    /// </summary>
    public static long Present()
    {
        var rc = PresentCheckRaw();
        if (rc != 0) { Check(rc); return rc; }
        long w = 0, h = 0;
        IntPtr px = SceneFrame(ref w, ref h);
        if (px == IntPtr.Zero || w <= 0 || h <= 0) Fail("SCENE PRESENT: nessun frame (manca VIEW o RENDER)");
        if (GfxPresentArgb(px, (int)w, (int)h) < 0)
            Fail("SCENE PRESENT: nessuna pagina 2D attiva (SCREEN o CANVAS USE prima)");
        return 0;
    }

    [DllImport(Lib, EntryPoint = "gmb_scene_render", CallingConvention = CallingConvention.Cdecl)]
    private static extern long RenderRaw();
    public static long Render() { var rc = RenderRaw(); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_save", CallingConvention = CallingConvention.Cdecl)]
    private static extern long SaveRaw([MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    public static long Save(string path) { var rc = SaveRaw(path); Check(rc); return rc; }

    // --- letture (funzioni: ritornano un valore) ------------------------------------------
    [DllImport(Lib, EntryPoint = "gmb_scene_verts", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Verts();

    [DllImport(Lib, EntryPoint = "gmb_scene_tris", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Tris();

    [DllImport(Lib, EntryPoint = "gmb_scene_drawn", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Drawn();

    [DllImport(Lib, EntryPoint = "gmb_scene_culled", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Culled();

    [DllImport(Lib, EntryPoint = "gmb_scene_pixels", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Pixels();

    [DllImport(Lib, EntryPoint = "gmb_scene_hash", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Hash();

    [DllImport(Lib, EntryPoint = "gmb_scene_pixel", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Pixel(long x, long y);

    // ── P5: superficie interattiva (specifica §3.3) ─────────────────────────────────────────
    // Corpo, movimento, quote e zone vivono solo nel runtime C (gmb_3d_scene_actor.c):
    // qui si inoltra, come per gli altri verbi SCENE (§0bis).
    [DllImport(Lib, EntryPoint = "gmb_scene_player", CallingConvention = CallingConvention.Cdecl)]
    private static extern long PlayerRaw(double radius, double height, double eye, double step);
    public static long Player(double radius, double height, double eye, double step)
    { var rc = PlayerRaw(radius, height, eye, step); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_move", CallingConvention = CallingConvention.Cdecl)]
    private static extern long MoveRaw(double forward, double right);
    public static long Move(double forward, double right)
    { var rc = MoveRaw(forward, right); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_turn", CallingConvention = CallingConvention.Cdecl)]
    private static extern long TurnRaw(double dYaw, double dPitch);
    public static long Turn(double dYaw, double dPitch)
    { var rc = TurnRaw(dYaw, dPitch); Check(rc); return rc; }

    [DllImport(Lib, EntryPoint = "gmb_scene_lift", CallingConvention = CallingConvention.Cdecl)]
    private static extern long LiftRaw(double dy);
    public static long Lift(double dy) { var rc = LiftRaw(dy); Check(rc); return rc; }

    // SCENE ZONE id, x1, y1, x2, y2, z1, z2 [, tag]: tag NULL quando assente.
    [DllImport(Lib, EntryPoint = "gmb_scene_zone", CallingConvention = CallingConvention.Cdecl)]
    private static extern long ZoneRaw(long id, double x1, double y1, double x2, double y2,
                                       double z1, double z2, [MarshalAs(UnmanagedType.LPUTF8Str)] string? tag);
    public static long Zone(long id, double x1, double y1, double x2, double y2,
                            double z1, double z2, string? tag)
    { var rc = ZoneRaw(id, x1, y1, x2, y2, z1, z2, tag); Check(rc); return rc; }

    // Letture: camera (X/Y/Z = occhio), corpo (EYE, HIT), quote e zone.
    [DllImport(Lib, EntryPoint = "gmb_scene_x", CallingConvention = CallingConvention.Cdecl)]
    public static extern double X();
    [DllImport(Lib, EntryPoint = "gmb_scene_y", CallingConvention = CallingConvention.Cdecl)]
    public static extern double Y();
    [DllImport(Lib, EntryPoint = "gmb_scene_z", CallingConvention = CallingConvention.Cdecl)]
    public static extern double Z();
    [DllImport(Lib, EntryPoint = "gmb_scene_eye", CallingConvention = CallingConvention.Cdecl)]
    public static extern double Eye();
    [DllImport(Lib, EntryPoint = "gmb_scene_yaw", CallingConvention = CallingConvention.Cdecl)]
    public static extern double Yaw();
    [DllImport(Lib, EntryPoint = "gmb_scene_pitch", CallingConvention = CallingConvention.Cdecl)]
    public static extern double Pitch();
    [DllImport(Lib, EntryPoint = "gmb_scene_hit", CallingConvention = CallingConvention.Cdecl)]
    public static extern double Hit();

    [DllImport(Lib, EntryPoint = "gmb_scene_floor_at", CallingConvention = CallingConvention.Cdecl)]
    public static extern double FloorAt(double x, double y, double z);
    [DllImport(Lib, EntryPoint = "gmb_scene_ceil_at", CallingConvention = CallingConvention.Cdecl)]
    public static extern double CeilAt(double x, double y, double z);
    [DllImport(Lib, EntryPoint = "gmb_scene_zone_at", CallingConvention = CallingConvention.Cdecl)]
    public static extern long ZoneAt(double x, double y, double z);

    [DllImport(Lib, EntryPoint = "gmb_scene_zonetag_text", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr ZoneTagRaw(long id);
    public static string ZoneTag(long id)
    {
        var p = ZoneTagRaw(id);
        return p == IntPtr.Zero ? "" : (Marshal.PtrToStringUTF8(p) ?? "");
    }

    // PICK(x, y): id dell'oggetto nel pixel dal buffer di SCENE PICK.
    [DllImport(Lib, EntryPoint = "gmb_scene_pick", CallingConvention = CallingConvention.Cdecl)]
    public static extern long Pick(long x, long y);
}
