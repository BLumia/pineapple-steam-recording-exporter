# Pineapple Steam Recording Exporter

## Description

This is a tool for exporting video recorded by Steam as MP4 files in a fastest way and without losing any quality, and also allows you share exported MP4 videos to another device via local network.

This tool is designed to be used on Steam Deck, but also works on other Linux distributions and Windows as well.

## Get it!

We are not ready for stable release yet, the goal is to put release binary on Flathub for everyone to download, and also on GitHub Release for other users (e.g. Windows users). I'll update this section once we have a proper release.

Meanwhile you can check out the nightly builds, which might have some known issues but already usable. See info below.

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
