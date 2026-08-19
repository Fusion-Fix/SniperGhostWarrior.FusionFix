# Sniper: Ghost Warrior Fusion Fix
<img width="5533" height="1687" alt="image (3)" src="https://github.com/user-attachments/assets/58ff21d4-be3a-4d95-a96e-412388c429e2" />


## Installation
The latest version of [Sniper: Ghost Warrior Fusion Fix](https://github.com/Fusion-Fix/SniperGhostWarrior.FusionFix/releases) can be found in the Releases page.

### Game Setup
- After downloading Sniper: Ghost Warrior Fusion Fix, extract the contents to your Sniper: Ghost Warrior directory and overwrite all existing files when prompted.
- You can adjust the mod settings inside `SniperGhostWarrior.FusionFix.ini` located in the `plugins` folder. The file is watched while the game runs, so most settings apply as soon as you save it.

## Features

### Display
- **Display Mode** - Sets the display mode: (0) fullscreen, (1) windowed.
- **V-Sync** - enabled or disables Vsync: (0) off, (1) on.
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
See [CONTRIBUTING.md](CONTRIBUTING.md) for workflow and reverse-engineering note conventions.

## License

[MIT](LICENSE)
