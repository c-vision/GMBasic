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
    public static void SceneVertexN(double x, double y, double z, double u, double v,
                                    double nx, double ny, double nz)
        => CommonLib.Scene3DNative.VertexN(x, y, z, u, v, nx, ny, nz);

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
    public static void SceneAa(long on) => CommonLib.Scene3DNative.Aa(on);

    // SCENE PICK on|off: buffer di id per pixel, additivo e spento di default.
    public static void ScenePickEnable(long on) => CommonLib.Scene3DNative.PickEnable(on);
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

    // P5/M-KEY: chiave TRS dell'oggetto e tempo di scena (RENDER campiona le piste).
    public static void SceneKey(long o, double t, double x, double y, double z,
                                double yaw, double pitch, double roll)
        => CommonLib.Scene3DNative.Key(o, t, x, y, z, yaw, pitch, roll);
    public static void SceneTime(double t) => CommonLib.Scene3DNative.SceneTime(t);

    // P5/M-LOD: oltre `dist` l'oggetto `o` disegna la geometria dell'oggetto `mesh`.
    public static void SceneLod(long o, double dist, long mesh)
        => CommonLib.Scene3DNative.Lod(o, dist, mesh);

    // P5/M-INST: geometria e materiale dell'oggetto `src`, trasformazione e tinta di `o`.
    public static void SceneInstance(long o, long src)
        => CommonLib.Scene3DNative.Instance(o, src);

    // SCENE PRESENT: target 3D → pagina 2D corrente (il present vero resta al main thread).
    public static void ScenePresent() => CommonLib.Scene3DNative.Present();

    public static void SceneRender() => CommonLib.Scene3DNative.Render();
    public static void SceneSave(string path) => CommonLib.Scene3DNative.Save(path);

    // --- P5: superficie interattiva (specifica §3.3) ----------------------------------------
    // Corpo, movimento, quote e zone vivono solo nel runtime C: qui si inoltra.
    public static void ScenePlayer(double radius, double height, double eye, double step)
        => CommonLib.Scene3DNative.Player(radius, height, eye, step);
    public static void SceneMove(double forward, double right)
        => CommonLib.Scene3DNative.Move(forward, right);
    public static void SceneTurn(double dYaw, double dPitch)
        => CommonLib.Scene3DNative.Turn(dYaw, dPitch);
    public static void SceneLift(double dy) => CommonLib.Scene3DNative.Lift(dy);
    public static void SceneZone(long id, double x1, double y1, double x2, double y2,
                                 double z1, double z2, string? tag)
        => CommonLib.Scene3DNative.Zone(id, x1, y1, x2, y2, z1, z2, tag);

    // --- letture (funzioni: ritornano un valore) --------------------------------------------
    public static double SceneVerts() => CommonLib.Scene3DNative.Verts();
    public static double SceneTris() => CommonLib.Scene3DNative.Tris();
    public static double SceneDrawn() => CommonLib.Scene3DNative.Drawn();
    public static double SceneCulled() => CommonLib.Scene3DNative.Culled();
    public static double ScenePixels() => CommonLib.Scene3DNative.Pixels();
    public static double SceneHash() => CommonLib.Scene3DNative.Hash();
    public static double ScenePixel(long x, long y) => CommonLib.Scene3DNative.Pixel(x, y);
    // PICK(x, y): id dell'oggetto nel pixel (0 fondo, 1..256 oggetto,
    // -1 senza target o fuori target, -2 buffer spento).
    public static double ScenePick(long x, long y) => CommonLib.Scene3DNative.Pick(x, y);

    // P5: camera, corpo, quote e zone. ZONETAG ritorna una stringa (il tag).
    public static double SceneX() => CommonLib.Scene3DNative.X();
    public static double SceneY() => CommonLib.Scene3DNative.Y();
    public static double SceneZ() => CommonLib.Scene3DNative.Z();
    public static double SceneEye() => CommonLib.Scene3DNative.Eye();
    public static double SceneYaw() => CommonLib.Scene3DNative.Yaw();
    public static double ScenePitch() => CommonLib.Scene3DNative.Pitch();
    public static double SceneHit() => CommonLib.Scene3DNative.Hit();
    public static double SceneFloorAt(double x, double y, double z) => CommonLib.Scene3DNative.FloorAt(x, y, z);
    public static double SceneCeilAt(double x, double y, double z) => CommonLib.Scene3DNative.CeilAt(x, y, z);
    public static double SceneZoneAt(double x, double y, double z) => CommonLib.Scene3DNative.ZoneAt(x, y, z);
    public static string SceneZoneTag(long id) => CommonLib.Scene3DNative.ZoneTag(id);

    // P5/M-RAYS: raggi. SHOOT/USE tornano l'id (−1 = niente); SHOT* l'ultimo colpo.
    public static double SceneShoot(double range) => CommonLib.Scene3DNative.Shoot(range);
    public static double SceneUse(double reach) => CommonLib.Scene3DNative.Use(reach);
    public static double SceneSees(long thing) => CommonLib.Scene3DNative.Sees(thing);
    public static double SceneShotKind() => CommonLib.Scene3DNative.ShotKind();
    public static double SceneShotObj() => CommonLib.Scene3DNative.ShotObj();
    public static double SceneShotDist() => CommonLib.Scene3DNative.ShotDist();
    public static double SceneShotX() => CommonLib.Scene3DNative.ShotX();
    public static double SceneShotY() => CommonLib.Scene3DNative.ShotY();
    public static double SceneShotZ() => CommonLib.Scene3DNative.ShotZ();

    // P5/M-STATS: una misura della scena per nome (−1 = chiave sconosciuta).
    public static double SceneStats(string name) => CommonLib.Scene3DNative.Stats(name);
}
