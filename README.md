# Spine2Dump
Turn jumbled **Spine2D** rips into their correct sprites.

Supports Spine `3.5`, `3.6`, `3.7`, `3.8`, `4.0`, `4.1`, and `4.2`.

## Usage

List and validate the assets in a folder
```shell
spine2dump scan ./samples
```

Dump sampled animation frames as numbered PNGs
```shell
spine2dump dump ./samples -o ./dump --animation idle --start 0 --end 2 --fps 30
```

### Examples

```shell
# Recursively scan a folder and validate its atlas PNG pages
spine2dump scan ./assets

# Print skeleton info and list every animation
spine2dump inspect ./assets

# List the detected expression attachment candidates
spine2dump expressions ./assets

# Render one full-pose PNG per detected expression
spine2dump dump-expressions ./assets -o ./dump/expressions

# Dump 2 seconds of the idle animation at 30 fps
spine2dump dump ./assets -o ./dump --animation idle --start 0 --end 2 --fps 30

# Crop all frames of an animation to one shared box with 8px padding
spine2dump dump ./assets -o ./dump --animation idle --trim-mode animation --trim-padding 8

# Validate the inputs without writing any images
spine2dump dump ./assets -o ./dump --animation idle --dry-run

# Render onto a 1024x1024 canvas with an extra 1.5x scale
spine2dump dump ./assets -o ./dump --size 1024 --scale 1.5

# Render onto a non-square canvas and crop transparent borders
spine2dump dump ./assets -o ./dump --width 1280 --height 720 --trim
```

Animation export writes one output folder per animation, containing numbered PNG frames. Video export can be added later by piping those frames to FFmpeg.

<details>
  <summary>Command Line</summary>

### `spine2dump --help`

| Command             | Description                                          |
|---------------------|------------------------------------------------------|
| `scan`              | Recursively scan a folder and validate atlas pages   |
| `inspect`           | Print skeleton info and list animations              |
| `expressions`       | List detected expression attachment candidates       |
| `dump-expressions`  | Render one full-pose PNG per expression candidate    |
| `dump`              | Dump sampled animation frames                        |
| `--help`            | Print help                                           |

---

### `spine2dump dump <asset-dir> --help`

| Option                        | Description                                   | Default             |
|-------------------------------|-----------------------------------------------|---------------------|
| `-o`, `--output <output-dir>` | Output directory for the dumped frames        |                     |
| `--animation <name>`          | Animation name to dump                        |                     |
| `--start <seconds>`           | Start time in seconds                         | `0`                 |
| `--end <seconds>`             | End time in seconds                           |                     |
| `--fps <value>`               | Frames per second                             | `30`                |
| `--size <px>`                 | Square output canvas size                     |                     |
| `--width <px>`                | Output canvas width                           |                     |
| `--height <px>`               | Output canvas height                          |                     |
| `--scale <value>`             | Render scale multiplier applied after fit     | `1.0`               |
| `--trim`                      | Crop transparent borders per image            |                     |
| `--trim-mode <mode>`          | Animation crop behavior                       | `none`, `frame`, `animation` |
| `--trim-padding <px>`         | Padding kept around trimmed bounds            | `0`                 |
| `--alpha-threshold <0-255>`   | Minimum alpha counted as visible              |                     |
| `--dry-run`                   | Validate only, write nothing                  |                     |
| `--help`                      | Print help                                    |                     |

---

### `spine2dump dump-expressions <asset-dir> --help`

| Option                        | Description                               | Default |
|-------------------------------|-------------------------------------------|---------|
| `-o`, `--output <output-dir>` | Output directory for the dumped images    |         |
| `--size <px>`                 | Square output canvas size                 |         |
| `--width <px>`                | Output canvas width                       |         |
| `--height <px>`               | Output canvas height                      |         |
| `--scale <value>`             | Render scale multiplier                   | `1.0`   |
| `--trim`                      | Crop transparent borders                  |         |
| `--trim-padding <px>`         | Padding kept around trimmed bounds        | `0`     |
| `--alpha-threshold <0-255>`   | Minimum alpha counted as visible          |         |
| `--help`                      | Print help                                |         |

</details>

## Expressions

Spine does not have one universal "expression" asset type. Depending on the rig, expressions may be separate animations, skins, slots, or attachment variants. The included sample stores expressions as attachments on the `00_Default` slot, with `00_Eyeclose` as a separate eye-close attachment.

List the detected expression candidates
```shell
spine2dump expressions ./samples
```

Render one full-pose PNG per detected expression candidate
```shell
spine2dump dump-expressions ./samples -o ./dump/expressions
```

The CPU renderer supports region attachments, mesh attachments, clipping attachments, slot/attachment tint, and normal/additive/multiply/screen blend modes. Two-color tint-black data is currently rendered as regular tint.

## Building

1. Install [CMake](https://cmake.org), [Ninja](https://ninja-build.org), and [Clang](https://clang.llvm.org)
2. Clone this repository
```sh
git clone https://github.com/Deathemonic/Spine2Dump
cd Spine2Dump
```
3. Build using `cmake`
```sh
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang
cmake --build build
```

The executable is generated at `build/spine2dump`.

To change the embedded runtime list, pass `SPINE_VERSIONS`:
```sh
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DSPINE_VERSIONS="3.8;4.2"
```

### Contributing
Don't like my [shitty code](https://www.reddit.com/r/programminghorror) and what to change it? Feel free to contribute by submitting a **pull request** or **issue**. Always appreciate the help.

### Acknowledgement
- [EsotericSoftware/spine-runtimes](https://github.com/EsotericSoftware/spine-runtimes)
