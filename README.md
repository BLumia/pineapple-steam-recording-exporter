# Pineapple Steam Recording Exporter

## Description

This is a tool for exporting video recorded by Steam as MP4 files in a fastest way and without losing any quality, and also allows you share exported MP4 videos to another device via local network.

This tool is designed to be used on Steam Deck, but also works on other Linux distributions and Windows as well.

## Get it!

For Linux users, the Flatpak version on Flathub is suggested to be used:

<a href='https://flathub.org/apps/net.blumia.pineapple-steam-recording-exporter'>
<img width='240' alt='Get it on Flathub' src='https://flathub.org/api/badge?locale=en'/>
</a>

If using Flatpak version is not an option, you can get other builds at [GitHub Release](https://github.com/BLumia/pineapple-steam-recording-exporter/releases) page (e.g. Windows version, AppImage version), but be aware non-flatpak version might have known issues.

You can also check out the nightly builds if preferred, see info below.

<details>
<summary>CI builds:</summary>

|CI|Build Status|CI Builds|
|---|---|---|
|Windows|[![Windows CI](https://github.com/BLumia/pineapple-steam-recording-exporter/actions/workflows/build.yml/badge.svg)](https://github.com/BLumia/pineapple-steam-recording-exporter/actions/workflows/build.yml)|[Get Latest CI Builds](https://nightly.link/BLumia/pineapple-steam-recording-exporter/workflows/build/master)|
|Linux (AppImage & Flatpak)|[![Linux CI](https://github.com/BLumia/pineapple-steam-recording-exporter/actions/workflows/build-linux.yml/badge.svg)](https://github.com/BLumia/pineapple-steam-recording-exporter/actions/workflows/build.yml)|[Get Latest CI Builds](https://nightly.link/BLumia/pineapple-steam-recording-exporter/workflows/build-linux/master)|

</details>

## How it works

The tool uses FFmpeg to convert the video files recorded by Steam into MP4 format without video transcoding and without losing any quality (`ffmpeg -c copy`). It also provides a built-in HTTP server to share the exported videos over the local network.

## Contribution

Beside feedback and code contribution, other contributions are also welcome!

### Help Translation!

[Translate into your language on Codeberg's Weblate instance](https://translate.codeberg.org/projects/pineapple-apps/pineapple-steam-recording-exporter/)

### Funding

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/blumia)

[![Afdian](https://static.afdiancdn.com/static/img/logo/logo.png)Afdian](https://afdian.com/a/BLumia)

## CLA

```
By sending patches in GitHub Pull Request, Issues, email patch 
set, or any other form to this project, it is assumed that you
are offering the Pineapple Tracker Player project and the original
project author (Gary Wang) unlimited, non-exclusive right to
reuse, modify, and relicense the code.
```

This is important because the inability to relicense code has caused devastating problems for other Free Software projects (such as KDE and NASM). Pineapple Tracker Player will always be available in an OSI approved, DFSG-compatible license. If you wish to specify special license conditions of your contributions, just say so when you send them.

## License

The source code of this project is licensed under [**GNU General Public License v3.0 only**](https://spdx.org/licenses/GPL-3.0-only.html) license. Individual files may have a different, but compatible license.
