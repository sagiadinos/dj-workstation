# Building the AppImage

The AppImage is built on **Debian 13 (Trixie), x86_64** with its stock GCC 14. It runs on every distribution with
glibc 2.41 or newer: Debian 13, Ubuntu 26.04, Linux Mint 23, Fedora 42+, Arch, openSUSE Tumbleweed.

A clean Trixie system, VM or container (`docker run -it debian:trixie`) is enough.

## 1. Install the packages

```sh
sudo apt install --no-install-recommends \
  build-essential cmake ninja-build git pkg-config ca-certificates wget file desktop-file-utils \
  libasound2-dev libpulse-dev libpipewire-0.3-dev libjack-jackd2-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
  libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev \
  libdrm-dev libgbm-dev libegl-dev libgl-dev libgles-dev libdbus-1-dev libudev-dev libibus-1.0-dev
```

- `build-essential`, `cmake`, `ninja-build`, `git`: the toolchain (GCC 14, CMake 3.31)
- `libasound2-dev`: RtMidi links ALSA directly
- The X11, Wayland, audio, D-Bus and udev headers are only needed so that SDL3 is built with these backends. SDL
  loads the libraries at run time with `dlopen` from the user's system, so none of them goes into the AppImage.

Do **not** install `libsdl3-dev`, `libtag-dev` or `libsoundtouch-dev`: the build downloads SDL3, SDL3_ttf (with
FreeType and HarfBuzz), TagLib and SoundTouch in fixed versions and links them statically.

## 2. Build

```sh
git clone https://github.com/sagiadinos/dj-workstation.git
cd dj-workstation
cmake --preset linux-release -B build/appimage \
  -DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER \
  -DDJW_SYSTEM_SOUNDTOUCH=OFF \
  -DBUILD_SHARED_LIBS=OFF -DSDL_SHARED=OFF -DSDL_STATIC=ON \
  -DDJW_BUILD_TESTS=OFF
cmake --build build/appimage
```

- `FETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER` and `DJW_SYSTEM_SOUNDTOUCH=OFF`: always use the downloaded
  dependencies, never something that happens to be installed
- `BUILD_SHARED_LIBS=OFF`, `SDL_SHARED=OFF`, `SDL_STATIC=ON`: everything static, so there are no libraries to bundle

Check that the binary only needs libraries every system has:

```sh
readelf -d build/appimage/src/player-ui/sdl/djw | grep NEEDED
```

Expected: `libasound.so.2`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6`. If `libSDL3` or `libtag`
shows up, a dependency was taken from the system or built shared.

## 3. Put together the AppDir

The program finds `fonts/` and `mappings/` next to its binary (`SDL_GetBasePath()`), so they go into `usr/bin`.

```sh
B=build/appimage/src/player-ui/sdl
rm -rf AppDir && mkdir -p AppDir/usr/bin
cp "$B/djw" AppDir/usr/bin/
cp -r "$B/fonts" "$B/mappings" AppDir/usr/bin/
cp "$B/com.dj_workstation.djw.desktop" AppDir/
cp misc/logo/djw-icon.svg AppDir/com.dj_workstation.djw.svg
ln -s usr/bin/djw AppDir/AppRun
desktop-file-validate AppDir/com.dj_workstation.djw.desktop
```

## 4. Build the AppImage

```sh
wget https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x appimagetool-x86_64.AppImage
ARCH=x86_64 ./appimagetool-x86_64.AppImage --appimage-extract-and-run AppDir DJ-Workstation-x86_64.AppImage
```

`--appimage-extract-and-run` lets appimagetool run without FUSE (containers). The result is
`DJ-Workstation-x86_64.AppImage`.

## 5. Test

```sh
./DJ-Workstation-x86_64.AppImage
```

The AppImage runtime needs no `libfuse2`, only `fusermount` (package `fuse3`, present on every desktop). Test at least
on X11 and Wayland, with ALSA and PipeWire, and with a MIDI controller.
