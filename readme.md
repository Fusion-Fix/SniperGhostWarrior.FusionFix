# SniperGhostWarrior.FusionFix

<img width="5533" height="1687" alt="image (6)" src="https://github.com/user-attachments/assets/38a14f15-1585-482e-bf9c-d192943e3cb7" />


## Installation

1. Download the latest release from the [Releases](https://github.com/Fusion-Fix/SniperGhostWarrior.FusionFix/releases/latest) page.
2. Extract the archive into the game root directory.
3. Launch the game.

## Features

### Display
- **Display Mode** - Sets the display mode: (0) fullscreen, (1) windowed.
- **V-Sync** - Enables or disables Vsync: (0) off, (1) on.
- **Internal Resolution** - Added the option to set the internal resolution without it affecting the window resolution, useful for supersampling and downscaling.
- **Scaling Filter** -  Sets the internal resolution scaling filter.
- **Max Frame Rate** - Frame rate cap: (0) uncapped, (1) the rate the display is running at.

### Graphics
- **Anisotropic Filtering** - Forces the selected AF level, range: 2 to 16.

### Gameplay
- **No Detection Indicator** - Removes the detection indicator.
- **No Waypoint Marker** - Removes the world markers pointing at objectives.

### Field of View
- **Field of View** - The player camera's horizontal FOV in degrees, range: 45 to 140. Does not affect scopes or cutscenes.

<div align="center">
  <table>
    <tr>
      <td width="50%"><img style="width:100%" src="https://github.com/user-attachments/assets/f0f76ba2-113d-4778-81b0-5da9820fde7b"></td>
      <td width="50%"><img style="width:100%" src="https://github.com/user-attachments/assets/150c12ad-c96f-4ea2-9e6b-fe02202cab31"></td>
    </tr>
    <tr>
      <td align="center">Before</td>
      <td align="center">After</td>
    </tr>
  </table>
</div>

### General
- **Skip Intro** - Skips the publisher logo, the intro movie and the "press start" screen, booting straight to the main menu.
- **Skip Press Any Key** - Skips the "press any key" prompt.

### Fixes
- Fixed an issue where HUD elements wouldn't scale properly at higher resolutions.
- Fixed the menu movies playing back greyscale and upside down.

<div align="center">
  <table>
    <tr>
      <td width="50%"><img style="width:100%" src="https://github.com/user-attachments/assets/0140bf7f-ca69-4c3c-b5b6-38d9f34cd861"></td>
      <td width="50%"><img style="width:100%" src="https://github.com/user-attachments/assets/f0f7bd65-29ed-4af8-85b0-abee20a95435"></td>
    </tr>
    <tr>
      <td align="center">Before</td>
      <td align="center">After</td>
    </tr>
  </table>
</div>

## Building from Source

Requirements:
- Visual Studio 2022 or 2026 (with C++ desktop workload)
- Git (for submodule checkout)

```bat
git clone --recurse-submodules https://github.com/Fusion-Fix/SniperGhostWarrior.FusionFix
cd SniperGhostWarrior.FusionFix
premake5.bat
```

Open `build/SniperGhostWarrior.FusionFix.slnx` in Visual Studio and build.

## Contributing

Pull requests are welcome. Please open an issue first to discuss what you would like to change.
See [contributing.md](contributing.md) for workflow and reverse-engineering note conventions.

## License

[GPL-3.0-only](license)
