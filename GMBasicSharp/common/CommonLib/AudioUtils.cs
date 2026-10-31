// FILE: CommonLib.AudioUtils.cs

/* 
 * AudioUtils.cs: utilità per il calcolo di parametri audio (envelope ADSR, frequenze MIDI)
 * 
 * FUNZIONI:
 * - CalculateEnvelopeADSR(sample, attackSamples, decaySamples, sustainSamples, releaseSamples, sustainLevel): calcola envelope ADSR a 4 fasi
 * - CalculateEnvelope(sample, totalSamples, attackSamples, releaseSamples): calcola envelope per suono naturale
 * - CalculateFrequency(baseFreq, octave): calcola la frequenza per una nota in una specifica ottava
 * - MidiNoteToFrequency(midiNote): converte una nota MIDI in frequenza
 * - CalculateNoteDuration(noteLength, tempo): calcola la durata di una nota
 */

namespace CommonLib;

public static class AudioUtils
{
    /// <summary>
    /// Calcola envelope ADSR a 4 fasi.
    /// </summary>
    public static double CalculateEnvelopeADSR(int sample, int attackSamples,
        int decaySamples, int sustainSamples, int releaseSamples, double sustainLevel)
    {
        if (attackSamples > 0 && sample < attackSamples)
            return (double)sample / attackSamples;

        var afterAttack = sample - attackSamples;
        if (decaySamples > 0 && afterAttack < decaySamples)
            return 1.0 - (1.0 - sustainLevel) * afterAttack / decaySamples;

        var afterDecay = sample - attackSamples - decaySamples;
        if (sustainSamples > 0 && afterDecay < sustainSamples)
            return sustainLevel;

        var afterSustain = sample - attackSamples - decaySamples - sustainSamples;
        if (releaseSamples > 0 && afterSustain < releaseSamples && afterSustain >= 0)
            return sustainLevel * (1.0 - (double)afterSustain / releaseSamples);

        return 0.0;
    }

    /// <summary>
    /// Calcola l'envelope ADSR per un suono più naturale
    /// </summary>
    public static double CalculateEnvelope(int sample, int totalSamples, int attackSamples, int releaseSamples)
    {
        if (sample < attackSamples)
        {
            // Attack: volume cresce linearmente
            return (double)sample / attackSamples;
        }

        if (sample > totalSamples - releaseSamples)
        {
            // Release: volume decresce linearmente
            return (double)(totalSamples - sample) / releaseSamples;
        }

        // Sustain: volume costante
        return 1.0;
    }

    /// <summary>
    /// Calcola la frequenza per una nota in una specifica ottava
    /// </summary>
    public static double CalculateFrequency(double baseFreq, int octave)
    {
        return baseFreq * Math.Pow(2, octave - 4);
    }

    /// <summary>
    /// Converte una nota MIDI in frequenza
    /// </summary>
    public static double MidiNoteToFrequency(int midiNote)
    {
        return 440.0 * Math.Pow(2, (midiNote - 69) / 12.0);
    }

    /// <summary>
    /// Calcola la durata di una nota basata su lunghezza e tempo
    /// </summary>
    public static double CalculateNoteDuration(int noteLength, int tempo)
    {
        return 240.0 / (tempo * noteLength); // 240 = 60 * 4 (per note da 1/4)
    }
}
