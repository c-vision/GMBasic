// FILE: Interpreter.IlBackendRuntime.IlRuntime.Scene.cs
// Dominio SCENE (motore 3D, renderer mesh) sulla gamba IL: qui non c'è stato né logica, solo
// l'inoltro alla facciata `gmb_scene_*` del runtime C. I comandi sono void (§1: un comando non
// ritorna un valore); le letture ritornano double (il tipo del BASIC).

namespace Basic.IlBackend;

public static partial class IlRuntime
{
    // --- comandi ---------------------------------------------------------------------------
    public static void SceneReset() => CommonLib.Scene3DNative.Reset();
    public static void SceneView(long w, long h, double fovDeg) => CommonLib.Scene3DNative.View(w, h, fovDeg);

    public static void SceneTexture(long cells) => CommonLib.Scene3DNative.Texture(cells);

    // SCENE TEXTURE "file.png" (argomento stringa): stesso verbo, texture da PNG.
    public static void SceneTextureFile(string path) => CommonLib.Scene3DNative.TextureFile(path);

    public static void SceneObject(long r, long g, long b) => CommonLib.Scene3DNative.Object(r, g, b);

    public static void SceneVertex(double x, double y, double z, double u, double v)
        => CommonLib.Scene3DNative.Vertex(x, y, z, u, v);

    public static void SceneQuad(long a, long b, long c, long d) => CommonLib.Scene3DNative.Quad(a, b, c, d);

    public static void SceneBox(double x0, double y0, double z0, double x1, double y1, double z1)
        => CommonLib.Scene3DNative.Box(x0, y0, z0, x1, y1, z1);

    public static void SceneObjPos(double x, double y, double z, double yawDeg)
        => CommonLib.Scene3DNative.ObjPos(x, y, z, yawDeg);

    public static void SceneCamera(double x, double y, double z, double yawDeg, double pitchDeg)
        => CommonLib.Scene3DNative.Camera(x, y, z, yawDeg, pitchDeg);

    // SCENE LIGHT dx, dy, dz, livello: luce direzionale per faccia.
    public static void SceneLight(double dx, double dy, double dz, long level)
        => CommonLib.Scene3DNative.Light(dx, dy, dz, level);

    // Flag di materiale (idea #5).
    public static void SceneSmooth(long on) => CommonLib.Scene3DNative.Smooth(on);
    public static void SceneAlpha(long on) => CommonLib.Scene3DNative.Alpha(on);
    public static void SceneTwoSided(long on) => CommonLib.Scene3DNative.TwoSided(on);

    // P4: lampade point/spot, fog, FAR, cielo, colormap a 32 livelli, thing.
    public static void SceneLamp(long n, double x, double y, double z, double radius, double amount)
        => CommonLib.Scene3DNative.Lamp(n, x, y, z, radius, amount);
    public static void SceneLampSpot(long n, double x, double y, double z, double radius, double amount,
                                     double dx, double dy, double dz, double angle)
        => CommonLib.Scene3DNative.LampSpot(n, x, y, z, radius, amount, dx, dy, dz, angle);
    public static void SceneFog(double d) => CommonLib.Scene3DNative.Fog(d);
    public static void SceneFar(double d) => CommonLib.Scene3DNative.Far(d);
    public static void SceneSky(double repeat, double shift) => CommonLib.Scene3DNative.Sky(repeat, shift);
    public static void SceneColorMap() => CommonLib.Scene3DNative.ColorMap();
    public static void SceneThing(long n, double x, double y, double z, double w, double h)
        => CommonLib.Scene3DNative.Thing(n, x, y, z, w, h);
    public static void SceneObjectFlags(long r, long g, long b, long flags, long light)
        => CommonLib.Scene3DNative.ObjectFlags(r, g, b, flags, light);

    // SCENE PRESENT: target 3D → pagina 2D corrente (il present vero resta al main thread).
    public static void ScenePresent() => CommonLib.Scene3DNative.Present();

    public static void SceneRender() => CommonLib.Scene3DNative.Render();
    public static void SceneSave(string path) => CommonLib.Scene3DNative.Save(path);

    // --- letture (funzioni: ritornano un valore) --------------------------------------------
    public static double SceneVerts() => CommonLib.Scene3DNative.Verts();
    public static double SceneTris() => CommonLib.Scene3DNative.Tris();
    public static double SceneDrawn() => CommonLib.Scene3DNative.Drawn();
    public static double SceneCulled() => CommonLib.Scene3DNative.Culled();
    public static double ScenePixels() => CommonLib.Scene3DNative.Pixels();
    public static double SceneHash() => CommonLib.Scene3DNative.Hash();
    public static double ScenePixel(long x, long y) => CommonLib.Scene3DNative.Pixel(x, y);
}
