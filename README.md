# Liquid Glass Terminal

A GTK3 terminal emulator with a liquid glass aesthetic for Linux.  
Built and maintained by **Chokri Hammedi** ([@blue0x1](https://github.com/blue0x1)).

![Liquid Glass Terminal](liquid.png)

---

## Features

- Translucent frosted-glass panels with chromatic tinting
- KWin blur-behind support (Plasma desktop)
- Tabbed terminal sessions with rename and reorder
- Collapsible sidebar with tab navigator
- Live settings window — change theme color and glass opacity in real time
- macOS-style window controls
- 9 built-in color themes: Blue Frost, Graphite, Red, Blue, Yellow, Purple, Pink, Black, Gray
- Custom hex color picker
- Config persisted to `~/.config/liquid_glass/config`

---

## Install

### Debian / Ubuntu (.deb)

```bash
sudo dpkg -i liquid-glass_1.0.0_amd64.deb
```

### Build from source

**Dependencies:**

```bash
sudo apt install gcc libgtk-3-dev libvte-2.91-dev libx11-dev
```

**Compile:**

```bash
gcc -O2 -o liquid_glass_gtk liquid_glass_gtk.c \
  $(pkg-config --cflags --libs gtk+-3.0 vte-2.91) \
  -lX11 -lm
```

**Run:**

```bash
./liquid_glass_gtk
```

---

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

---

## Configuration

Config file: `~/.config/liquid_glass/config`

```ini
glassOpacity=0.12
themePreset=original
themeCustomHex=#050D1C
showSidebar=0
```

---

## License

MIT © 2025 Chokri Hammedi
