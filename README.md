<h1 align="center">Liquid Glass Terminal</h1>

<p align="center">A GTK3 terminal emulator with a liquid glass aesthetic for Linux.</p>

<p align="center">
  <a href="https://github.com/blue0x1/liquid-glass/releases">
    <img src="https://img.shields.io/github/downloads/blue0x1/liquid-glass/total?style=for-the-badge&label=Downloads&color=4a90d9" alt="Downloads"/>
  </a>
  <a href="https://github.com/blue0x1/liquid-glass/stargazers">
    <img src="https://img.shields.io/github/stars/blue0x1/liquid-glass?style=for-the-badge&color=4a90d9" alt="Stars"/>
  </a>
  <img src="https://img.shields.io/badge/version-1.0.0-4a90d9?style=for-the-badge" alt="Version"/>
  <img src="https://img.shields.io/badge/language-C-4a90d9?style=for-the-badge&logo=c" alt="Language"/>
  <img src="https://img.shields.io/badge/platform-Linux-4a90d9?style=for-the-badge&logo=linux&logoColor=white" alt="Platform"/>
</p>

<p align="center">
  <img src="liquid.png" width="220" alt="Liquid Glass Terminal"/>
</p>

## Demo

https://github.com/user-attachments/assets/edaa4c62-0834-4f24-9c17-2383366994b6

## Screenshot

<p align="center">
  <img width="900" alt="Liquid Glass Terminal Screenshot" src="https://github.com/user-attachments/assets/b35b87b7-68ab-4c75-b69a-da56c5b23c2c" />
</p>

## Features

- Translucent frosted-glass panels with chromatic tinting
- Native KWin effect package for compositor-side backdrop capture, refraction, blur, and background contrast
- App-side OpenGL backdrop fallback for non-native sessions
- Tabbed terminal sessions with rename and reorder
- Collapsible sidebar with tab navigator
- Live settings window, change theme color and glass opacity in real time
- macOS-style window controls
- 9 built-in color themes: Blue Frost, Graphite, Red, Blue, Yellow, Purple, Pink, Black, Gray
- Custom hex color picker
- Config persisted to `~/.config/liquid_glass/config`

## Install

### Debian / Ubuntu (.deb)

```bash
sudo dpkg -i liquid-glass_1.0.0_amd64.deb
```

### Build from source

**Dependencies:**

```bash
sudo apt install gcc libgtk-3-dev libvte-2.91-dev libx11-dev libepoxy-dev libgl-dev xxd
```

**Compile:**

```bash
make
```

**Build .deb package:**

```bash
make deb
```

**Install app and native KWin effect:**

```bash
sudo make install
```

The install target builds and installs the native C++ KWin effect automatically,
then enables it for your Plasma session.

**Run:**

```bash
./liquid_glass_gtk
```

On Plasma, the native KWin effect captures the composited backdrop behind the
window, then applies refraction and frosted glass in the compositor.
The app-side X11 capture path is only a fallback for non-native cases.

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+T` | New tab |
| `Ctrl+W` | Close tab |
| `Ctrl+Tab` | Next tab |
| `Ctrl+Shift+Tab` | Previous tab |
| `Ctrl+Shift+C` | Copy |
| `Ctrl+Shift+V` | Paste |
| `Ctrl+\` | Toggle sidebar |
| `Ctrl+,` | Open settings |

## Configuration

Config file: `~/.config/liquid_glass/config`

```ini
glassOpacity=0.12
themePreset=original
themeCustomHex=#050D1C
showSidebar=0
```

## License

MIT © 2026 Chokri Hammedi
