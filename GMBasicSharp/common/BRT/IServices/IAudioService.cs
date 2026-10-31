// FILE: Basic.Runtime.IServices.IAudioService.cs

namespace Basic.Runtime;

/// <summary>Servizio audio (schema PCodeVM SoundManager). In headless, NullAudioService.</summary>
public interface IAudioService
{
    /// <summary>SOUND freq, dur [, volume]: beep su un voice (o master).</summary>
    void Sound(double frequency, double duration, int volume);
    /// <summary>SETFILTER voice, type, cutoff, resonance (0=Off,1=LowPass,2=HighPass,3=BandPass).</summary>
    void SetFilter(int voice, int type, double cutoff, double resonance);
    /// <summary>SOUND LOAD "file" AS #handle → handle (0 se fallisce).</summary>
    int LoadSound(string filePath);
    /// <summary>SOUND PLAY #handle [, channel] [, loops].</summary>
    void PlaySound(int handle, int channel, int loops);
    /// <summary>SOUND VOLUME n [, channel].</summary>
    void SetVolume(int volume, int? channel);
    /// <summary>PLAY "nota...": sequenza musicale (schema PCodeVM).</summary>
    void Play(string musicString);
    /// <summary>AUDIO action [, arg]: 0=Stop(All|canale), 1=PauseAll, 2=ResumeAll, 3=ClearQueue.
    /// arg può essere un numero o un nome di canale stringa (SOUND STOP "ch1").</summary>
    void AudioControl(int action, object? argument);
}
