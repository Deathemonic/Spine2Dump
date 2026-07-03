# Spine2Dump

C23 command-line scaffold for dumping Spine2D `.skel` / `.atlas` / `.png`
assets into animation frame images. The project embeds multiple official Spine C
runtimes into one executable and dispatches each input in-process based on the
`.skel` version.

Current status: the tool can recursively scan asset folders, validate atlas PNG
pages, load Spine 3.5, 3.6, 3.7, 3.8, 4.0, 4.1, and 4.2 binary skeletons, list
animations, list likely expression attachments, dump expression pose PNGs, and
dump sampled animation frames with a CPU renderer.

## Build with CMake

```sh
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang
cmake --build build
```

Generated files go in `build/`.

By default the build creates one executable:

```sh
build/spine2dump
```

It parses the primary `.skel` binary header, selects the matching embedded
runtime, and runs without subprocess workers. To change the embedded runtime
list:

```sh
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DSPINE2DUMP_SPINE_VERSIONS="3.8;4.2"
```

Dependencies are fetched with CMake FetchContent:

- `spine-runtimes`, branches selected by `SPINE2DUMP_SPINE_VERSIONS`
- `argtable3`, branch `master`
- `zf_log`, branch `master`
- `zlib`, tag `v1.3.1`
- `libspng`, tag `v0.7.4`

Source layout:

- `src/app`: command-line entry point
- `src/core`: asset discovery and Spine runtime backend
- `src/render`: CPU renderer and PNG image IO

## Run

```sh
./build/spine2dump --help
./build/spine2dump scan ./assets
./build/spine2dump inspect ./assets
./build/spine2dump expressions ./assets
./build/spine2dump dump-expressions ./assets -o ./dump/expressions
./build/spine2dump dump ./assets -o ./dump --animation idle --start 0 --end 2 --fps 30
./build/spine2dump dump ./assets -o ./dump --animation idle --trim-mode animation --trim-padding 8
./build/spine2dump dump ./assets -o ./dump --animation idle --dry-run
```

Animation export writes one output folder per animation, containing numbered PNG
frames. Video export can be added later by piping those frames to FFmpeg.

Render controls:

- `--size <px>` sets a square render canvas.
- `--width <px>` and `--height <px>` set a non-square render canvas.
- `--scale <value>` applies an extra multiplier after fit-to-canvas scaling.
- `--trim` crops transparent borders per output image.
- `--trim-mode animation` crops all frames of an animation to one shared box.
- `--trim-padding <px>` keeps transparent padding around trimmed output.
- `--alpha-threshold <0-255>` controls which pixels count as visible.

## Expressions

Spine does not have one universal "expression" asset type. Depending on the
rig, expressions may be separate animations, skins, slots, or attachment
variants. The included sample stores expressions as attachments on the
`00_Default` slot, with `00_Eyeclose` as a separate eye-close attachment.

Use this to list the detected expression candidates:

```sh
./build/spine2dump expressions ./samples
```

Use this to render one full-pose PNG per detected expression candidate:

```sh
./build/spine2dump dump-expressions ./samples -o ./dump/expressions
```

The current CPU renderer supports region attachments, mesh attachments, clipping
attachments, slot/attachment tint, and normal/additive/multiply/screen blend
modes. Two-color tint-black data is currently rendered as regular tint.
