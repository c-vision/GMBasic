// FILE: Interpreter.StdLib.Scene.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Scene — namespace 'SCENE' (motore 3D, renderer mesh)
 * 19 funzioni | 1 comando | 0 tipi | 0 costanti
 *
 * FUNZIONI (19): VERTS, TRIS, DRAWN, CULLED, PIXELS, HASH, PIXEL, PICK, X, Y, Z, EYE, YAW,
 *                PITCH, HIT, FLOORAT, CEILAT, ZONE, ZONETAG
 * COMANDO (1): SCENE (RESET | VIEW | TEXTURE | OBJECT | VERTEX | QUAD | BOX | OBJPOS | CAMERA |
 *                    PLAYER | MOVE | TURN | LIFT | ZONE | LIGHT | LAMP | FOG | FAR | SKY |
 *                    COLORMAP | THING | SMOOTH | ALPHA | TWOSIDED | PICK | RENDER | PRESENT | SAVE)
 * ==== GMB-STDLIB-AUDIT-END ==== */

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- SCENE NAMESPACE (motore 3D, renderer mesh — P3) ---
    // ========================================================================

    /// <summary>
    /// Registra il dominio <c>SCENE</c>: un comando legacy (tutti i verbi, dispatcher nel parser)
    /// e le sette LETTURE qualificate della scena mesh. Non c'è logica qui: ogni lettura è un
    /// builtin che i backend inoltrano alla facciata <c>gmb_scene_*</c> del runtime C
    /// (CommonLib.Scene3DNative per le gambe gestite, chiamata diretta per VM/C e C99Native) —
    /// una sola sorgente di stato, come per WORLD (§0bis).
    /// </summary>
    private void CreateSceneNamespace()
    {
        const string namespaceName = "SCENE";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic | SymbolFlags.IsFunction;
        const SymbolFlags noFlags = SymbolFlags.None;

        // --- comando: SCENE RESET|VIEW|TEXTURE|OBJECT|VERTEX|QUAD|BOX|OBJPOS|CAMERA|RENDER|SAVE ---
        // I verbi sono sottomandi del dispatcher: il comando è UNO solo (nessuna parentesi, §1).
        RegisterCommand(TokenKind.Scene, "SCENE", namespaceName, Parser.DispatchSceneStatement);

        // --- letture (funzioni: leggono e basta, §1) ---
        var procedures = new ProcedureDefinition[]
        {
            // VERTS(): vertici dichiarati nella scena (geometria accumulata, non disegnata).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SceneVerts, "VERTS"),
                [], new DoubleType(), TokenKind.SceneVerts),

            // TRIS(): triangoli dichiarati.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SceneTris, "TRIS"),
                [], new DoubleType(), TokenKind.SceneTris),

            // DRAWN(): triangoli che il rasterizzatore ha davvero disegnato dopo cull e clip.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SceneDrawn, "DRAWN"),
                [], new DoubleType(), TokenKind.SceneDrawn),

            // CULLED(): triangoli scartati dal frustum/back-face.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SceneCulled, "CULLED"),
                [], new DoubleType(), TokenKind.SceneCulled),

            // PIXELS(): pixel passati dal depth test nell'ultimo RENDER.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ScenePixels, "PIXELS"),
                [], new DoubleType(), TokenKind.ScenePixels),

            // HASH(): FNV-1a del frame (stesso frame ⇒ stesso numero: determinismo).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SceneHash, "HASH"),
                [], new DoubleType(), TokenKind.SceneHash),

            // PIXEL(x, y): colore 0x00RRGGBB del target, −1 fuori dai limiti.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ScenePixel, "PIXEL"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "x"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "y"), new LongType(), noFlags, null, false)
                ],
                new DoubleType(), TokenKind.ScenePixel),

            // PICK(x, y): id dell'oggetto nel pixel dal buffer di SCENE PICK
            // (0 fondo, 1..256 oggetto, −1 senza target o fuori target, −2 buffer spento).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ScenePick, "PICK"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "x"), new LongType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "y"), new LongType(), noFlags, null, false)
                ],
                new DoubleType(), TokenKind.ScenePick),

            // ── P5: superficie interattiva (specifica §3.3) ──────────────────────────
            // Camera e corpo. X/Y/Z sono l'occhio (CAMERA di P3); EYE è l'altezza
            // dell'occhio sui piedi, HIT l'ultimo MOVE.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SceneX, "X"),
                [], new DoubleType(), TokenKind.SceneX),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SceneY, "Y"),
                [], new DoubleType(), TokenKind.SceneY),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SceneZ, "Z"),
                [], new DoubleType(), TokenKind.SceneZ),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SceneEye, "EYE"),
                [], new DoubleType(), TokenKind.SceneEye),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SceneYaw, "YAW"),
                [], new DoubleType(), TokenKind.SceneYaw),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ScenePitch, "PITCH"),
                [], new DoubleType(), TokenKind.ScenePitch),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SceneHit, "HIT"),
                [], new DoubleType(), TokenKind.SceneHit),

            // Quote (M-FLOOR): punto (x, y, z), y = altezza; −1000000 se assente.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SceneFloorAt, "FLOORAT"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "z"), new DoubleType(), noFlags, null, false)
                ],
                new DoubleType(), TokenKind.SceneFloorAt),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SceneCeilAt, "CEILAT"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "z"), new DoubleType(), noFlags, null, false)
                ],
                new DoubleType(), TokenKind.SceneCeilAt),

            // Zone (M-ZONE): ZONE(x, y, z) = id della prima zona che contiene il
            // punto, −1 se nessuna; ZONETAG(id) = tag della zona, "" se assente.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SceneZone, "ZONE"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "z"), new DoubleType(), noFlags, null, false)
                ],
                new DoubleType(), TokenKind.SceneZone),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SceneZoneTag, "ZONETAG"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "id"), new LongType(), noFlags, null, false)
                ],
                new StringType(), TokenKind.SceneZoneTag)
        };

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
