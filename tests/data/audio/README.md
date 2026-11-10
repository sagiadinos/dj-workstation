# Test audio files

Generated sine tones, 0.25 s each, made with ffmpeg (no third-party recordings):

```bash
Q="-hide_banner -loglevel error -y -bitexact"
ffmpeg $Q -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=0.25" -ac 2 -c:a pcm_s16le -map_metadata -1 sine_440hz_48k_stereo.wav
ffmpeg $Q -i sine_440hz_48k_stereo.wav -c:a flac -map_metadata -1 sine_440hz_48k_stereo.flac
ffmpeg $Q -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=0.25" -ac 2 -c:a libmp3lame -b:a 128k -map_metadata -1 sine_440hz_48k_stereo.mp3
ffmpeg $Q -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=0.25" -ac 1 -c:a pcm_s16le -map_metadata -1 sine_440hz_44k1_mono.wav
```

With tags, for the library tests (ID3v2.4 in the MP3, Vorbis comments in the FLAC file):

```bash
ffmpeg $Q -i sine_440hz_48k_stereo.wav -c:a libmp3lame -b:a 128k -map_metadata -1 -id3v2_version 4 \
    -metadata artist="Tagged Artist" -metadata title="Tagged Title" -metadata album="Test Album" \
    -metadata genre="House" -metadata date="2024" tagged_id3v2.mp3
ffmpeg $Q -i sine_440hz_48k_stereo.wav -c:a flac -map_metadata -1 \
    -metadata artist="Beyoncé" -metadata title="Ünïcode Title" -metadata genre="Pop" -metadata date="2019" \
    tagged_vorbis.flac
```

ffmpeg's sine source has an amplitude of 1/8; the stereo files have 1/8 / √2, because `-ac 2` lowers the level by 3 dB. The FLAC file holds exactly the samples of the 48 kHz WAV file.
