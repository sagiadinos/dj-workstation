# Test audio files

Generated sine tones, 0.25 s each, made with ffmpeg (no third-party recordings):

```bash
Q="-hide_banner -loglevel error -y -bitexact"
ffmpeg $Q -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=0.25" -ac 2 -c:a pcm_s16le -map_metadata -1 sine_440hz_48k_stereo.wav
ffmpeg $Q -i sine_440hz_48k_stereo.wav -c:a flac -map_metadata -1 sine_440hz_48k_stereo.flac
ffmpeg $Q -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=0.25" -ac 2 -c:a libmp3lame -b:a 128k -map_metadata -1 sine_440hz_48k_stereo.mp3
ffmpeg $Q -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=0.25" -ac 1 -c:a pcm_s16le -map_metadata -1 sine_440hz_44k1_mono.wav
```

ffmpeg's sine source has an amplitude of 1/8; the stereo files have 1/8 / √2, because `-ac 2` lowers the level by 3 dB. The FLAC file holds exactly the samples of the 48 kHz WAV file.
