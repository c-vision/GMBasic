// FILE: Basic.Runtime.Services.NullAudioService.cs

namespace Basic.Runtime;

public class NullAudioService : IAudioService
{
    public void Sound(double frequency, double duration, int volume) { }
    public void SetFilter(int voice, int type, double cutoff, double resonance) { }
    public int LoadSound(string filePath) => 0;
    public void PlaySound(int handle, int channel, int loops) { }
    public void SetVolume(int volume, int? channel) { }
    public void Play(string musicString) { }
    public void AudioControl(int action, object? argument) { }
}
