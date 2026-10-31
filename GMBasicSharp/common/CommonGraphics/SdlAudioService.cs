// FILE: CommonGraphics.SdlAudioService.cs
// Implementazione reale (SDL_mixer) di IAudioService che delega ad AudioManager
// (CommonGraphics.AudioManager, schema PCodeVM SoundManager). Usata dal runner grafico
// SDL (test_graphics_headless); in headless si usa NullAudioService.

using Basic.Runtime;

namespace CommonGraphics;

public sealed class SdlAudioService : IAudioService
{
    private static readonly AudioManager Manager = new();

    public void Sound(double frequency, double duration, int volume)
        => Manager.Sound(frequency, duration, volume);
    public void SetFilter(int voice, int type, double cutoff, double resonance)
        => Manager.SetVoiceFilter(voice, type, cutoff, resonance);
    public int LoadSound(string filePath)
        => Manager.LoadAudioFile(filePath);
    public void PlaySound(int handle, int channel, int loops)
    {
        // PlayAudioFile(handle): riproduzione sul canale di default (loop non ancora esposto
        // dal manager). Channel/loops sono accettati per parità di firma.
        _ = channel; _ = loops;
        Manager.PlayAudioFile(handle);
    }
    public void SetVolume(int volume, int? channel)
        => Manager.SetVolume(volume, channel);
    public void Play(string musicString)
        => Manager.Play(musicString);
    public void AudioControl(int action, object? argument)
    {
        switch (action)
        {
            case 0: // Stop (canale nominato o All)
                if (argument is string name)
                    Manager.StopNamedAsync(name).GetAwaiter().GetResult();
                else
                    Manager.StopAllAsync().GetAwaiter().GetResult();
                break;
            case 1: Manager.PauseAll(); break;
            case 2: Manager.ResumeAll(); break;
            case 3: Manager.ClearQueue(); break;
        }
    }
}
