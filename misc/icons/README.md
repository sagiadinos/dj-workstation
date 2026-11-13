# Program icons

Made from `misc/logo/djw-icon.svg` by `tools/make_icons.py` (needs `rsvg-convert` and ImageMagick). Do not edit
the files here; change the SVG and run the script again.

| Platform | File | Use |
|---|---|---|
| Linux | `misc/logo/djw-icon.svg` | installed by CMake with the `.desktop` file (`misc/linux/`) |
| Windows | `windows/djw.ico` | 16 to 256 px; the icon of the `.exe` through a resource file (`IDI_ICON1 ICON "djw.ico"`) |
| macOS | `macos/djw.icns` | 16 to 1024 px in Apple's icon grid (824 of 1024 px, rounded corners); `CFBundleIconFile` of the app bundle |
| iOS | `ios/AppIcon-1024.png` | the one image of the asset catalog's AppIcon set; full square without transparency, iOS rounds it |
| Android | `android/res/` | copy into the app's `res/`: adaptive icon (`ic_launcher`, `ic_launcher_round`) with the record as foreground on black, PNGs for Android 7 and older; `android:icon="@mipmap/ic_launcher"`, `android:roundIcon="@mipmap/ic_launcher_round"` |
| Google Play | `android/play-store-512.png` | the store listing; full square without transparency |
