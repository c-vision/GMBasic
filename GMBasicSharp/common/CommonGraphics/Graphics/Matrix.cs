using CommonLib.Screens;
/*
// FILE: Interpreter.Graphics.Matrix.cs

using System.Numerics;

// ReSharper disable InconsistentNaming

namespace CommonGraphics.Graphics;

/// <summary>
/// Contiene l'implementazione per le operazioni su matrici di trasformazione 2D/3D.
/// Questa parte del GraphicsManager fornisce al linguaggio BASIC comandi in stile
/// DarkBASIC/Blitz per manipolare la posizione, rotazione e scala degli oggetti.
/// </summary>
public sealed partial class GraphicsManager
{
    // Il GraphicsManager deve mantenere un riferimento alle matrici
    // gestite dal programma BASIC. Usiamo un dizionario dove la chiave
    // è l'handle (ID) della matrice visibile al programmatore BASIC.
    private readonly Dictionary<int, Matrix4x4> _matrices = new();
    private int _nextMatrixId = 1;

    /// <summary>
    /// Uno stack per salvare e ripristinare le matrici.
    /// Ogni elemento dello stack è una tupla contenente l'ID della matrice
    /// e una copia del suo stato al momento del PUSH.
    /// </summary>
    private readonly Stack<(int matrixId, Matrix4x4 matrixState)> _matrixStack = new();
    
    /// <summary>
    /// La matrice di VISTA, che definisce la posizione e l'orientamento della telecamera.
    /// </summary>
    private Matrix4x4 _viewMatrix = Matrix4x4.Identity;

    /// <summary>
    /// La matrice di PROIEZIONE, che definisce come la scena 3D viene proiettata
    /// sullo schermo 2D (es. prospettica o ortografica).
    /// </summary>
    private Matrix4x4 _projectionMatrix = Matrix4x4.Identity;

    /// <summary>
    /// La matrice di MONDO, che definisce la trasformazione globale per tutti gli oggetti.
    /// Può essere usata per spostare o ruotare l'intera scena.
    /// </summary>
    private Matrix4x4 _worldMatrix = Matrix4x4.Identity;

    // --- Implementazioni Interne dei Comandi Matrice ---
    
    /// <summary>
    /// Crea una nuova matrice, la inizializza come matrice identità,
    /// e ne restituisce l'handle.
    /// </summary>
    /// <returns>L'handle (ID) della nuova matrice creata.</returns>
    private int CreateMatrixInternal()
    {
        var newId = _nextMatrixId++;
        _matrices[newId] = Matrix4x4.Identity;
        return newId;
    }

    /// <summary>
    /// Resetta una matrice esistente alla matrice identità.
    /// </summary>
    /// <param name="matrixId">L'handle della matrice da resettare.</param>
    private void IdentityMatrixInternal(int matrixId)
    {
        if (_matrices.ContainsKey(matrixId))
        {
            _matrices[matrixId] = Matrix4x4.Identity;
        }
    }

    /// <summary>
    /// Applica una traslazione a una matrice.
    /// </summary>
    /// <param name="matrixId">L'handle della matrice da modificare.</param>
    /// <param name="x">Traslazione sull'asse X.</param>
    /// <param name="y">Traslazione sull'asse Y.</param>
    /// <param name="z">Traslazione sull'asse Z.</param>
    private void TranslateMatrixInternal(int matrixId, float x, float y, float z)
    {
        if (_matrices.TryGetValue(matrixId, out var matrix))
        {
            _matrices[matrixId] = matrix * Matrix4x4.CreateTranslation(x, y, z);
        }
    }

    /// <summary>
    /// Applica una rotazione a una matrice usando gli angoli di Eulero (in radianti).
    /// </summary>
    /// <param name="matrixId">L'handle della matrice da modificare.</param>
    /// <param name="ax">Rotazione attorno all'asse X (pitch).</param>
    /// <param name="ay">Rotazione attorno all'asse Y (yaw).</param>
    /// <param name="az">Rotazione attorno all'asse Z (roll).</param>
    private void RotateMatrixInternal(int matrixId, float ax, float ay, float az)
    {
        if (_matrices.TryGetValue(matrixId, out var matrix))
        {
            var rotation = Matrix4x4.CreateFromYawPitchRoll(ay, ax, az);
            _matrices[matrixId] = matrix * rotation;
        }
    }

    /// <summary>
    /// Applica una scalatura a una matrice.
    /// </summary>
    /// <param name="matrixId">L'handle della matrice da modificare.</param>
    /// <param name="sx">Fattore di scala sull'asse X.</param>
    /// <param name="sy">Fattore di scala sull'asse Y.</param>
    /// <param name="sz">Fattore di scala sull'asse Z.</param>
    private void ScaleMatrixInternal(int matrixId, float sx, float sy, float sz)
    {
        if (_matrices.TryGetValue(matrixId, out var matrix))
        {
            _matrices[matrixId] = matrix * Matrix4x4.CreateScale(sx, sy, sz);
        }
    }
    
    /// <summary>
    /// Moltiplica due matrici e salva il risultato in una terza.
    /// </summary>
    /// <param name="matrixId1">L'handle della prima matrice.</param>
    /// <param name="matrixId2">L'handle della seconda matrice.</param>
    /// <param name="resultMatrixId">L'handle della matrice dove salvare il risultato.</param>
    private void MultiplyMatrixInternal(int matrixId1, int matrixId2, int resultMatrixId)
    {
        if (_matrices.TryGetValue(matrixId1, out var m1) && _matrices.TryGetValue(matrixId2, out var m2))
        {
            _matrices[resultMatrixId] = m1 * m2;
        }
    }

        /// <summary>
    /// Salva lo stato corrente di una matrice sullo stack.
    /// </summary>
    /// <param name="matrixId">L'handle della matrice da salvare.</param>
    private void MatrixPushInternal(int matrixId)
    {
        if (_matrices.TryGetValue(matrixId, out var matrixToPush))
        {
            _matrixStack.Push((matrixId, matrixToPush));
        }
        // Nota: Se l'ID non è valido, non facciamo nulla e non lanciamo un errore.
    }

    /// <summary>
    /// Ripristina l'ultimo stato di una matrice salvato sullo stack.
    /// Cerca nello stack l'ultimo salvataggio per l'ID specificato.
    /// </summary>
    /// <param name="matrixId">L'handle della matrice da ripristinare.</param>
    private void MatrixPopInternal(int matrixId)
    {
        // Controlla se lo stack non è vuoto.
        if (_matrixStack.Count == 0) return;
        
        // Cerca nello stack il salvataggio più recente che corrisponde all'ID.
        // Questo non è super efficiente, ma rispetta la logica di PUSH/POP per matrici specifiche.
        // Un'alternativa sarebbe uno stack per ogni matrice, ma sarebbe più complesso da gestire.
        
        // Estraiamo temporaneamente gli elementi per trovare l'ultimo corrispondente.
        var tempStack = new Stack<(int matrixId, Matrix4x4 matrixState)>();
        (int matrixId, Matrix4x4 matrixState)? foundState = null;

        while (_matrixStack.Count > 0)
        {
            var item = _matrixStack.Pop();
            if (item.matrixId == matrixId)
            {
                foundState = item;
                break; // Trovato l'ultimo PUSH per questo ID, esci.
            }
            tempStack.Push(item);
        }

        // Ripristina gli elementi che non corrispondevano.
        while (tempStack.Count > 0)
        {
            _matrixStack.Push(tempStack.Pop());
        }

        // Se abbiamo trovato uno stato, ripristiniamo la matrice.
        if (foundState.HasValue)
        {
            _matrices[foundState.Value.matrixId] = foundState.Value.matrixState;
        }
    }
    
    /// <summary>
    /// Imposta una matrice gestita dal BASIC come nuova matrice di VISTA.
    /// </summary>
    /// <param name="matrixId">L'handle della matrice da usare come matrice di vista.</param>
    private void SetViewMatrixInternal(int matrixId)
    {
        if (_matrices.TryGetValue(matrixId, out var matrix))
        {
            _viewMatrix = matrix;
        }
    }

    /// <summary>
    /// Imposta una matrice gestita dal BASIC come nuova matrice di PROIEZIONE.
    /// </summary>
    /// <param name="matrixId">L'handle della matrice da usare come matrice di proiezione.</param>
    private void SetProjectionMatrixInternal(int matrixId)
    {
        if (_matrices.TryGetValue(matrixId, out var matrix))
        {
            _projectionMatrix = matrix;
        }
    }
    
    /// <summary>
    /// Imposta una matrice gestita dal BASIC come nuova matrice di MONDO.
    /// </summary>
    /// <param name="matrixId">L'handle della matrice da usare come matrice di mondo.</param>
    private void SetWorldMatrixInternal(int matrixId)
    {
        if (_matrices.TryGetValue(matrixId, out var matrix))
        {
            _worldMatrix = matrix;
        }
    }
}
*/
