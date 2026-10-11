# AH-RAY Studio

A static website for **ah-ray.com**, with the hardware-free LED design environment at **`/studio/`**.

Studio runs the actual Dual S3 C++ effects and spatial pipeline, plus its bundled FastLED code, compiled to WebAssembly. Rendering runs inside a browser worker. No Python server, hardware connection, npm install, API service, account, or external CDN is needed to use the published site.

- 47 light effects and 25 spatial modes from FX package v8, with existing effect IDs preserved.
- All mode: 49 Dual nodes, 98 strips, 9,800 LEDs, including interior columns and horizontal runs.
- Wall mode: 32 Dual nodes, 64 strips, 6,400 LEDs. Each preset strip has span 9 and 100 LEDs.
- Orbit, top, and front views, with a floor and back wall for orientation.
- A/B output editing, up to eight virtual segments per output, pixel inspection, palette and effect controls.
- Pause, frame stepping, BPM beats, playback speed, blackout, and PNG snapshots.
- Sessions save in this browser's local storage; JSON export/import preserves the desktop emulator's `ahray-effects-lab-v1` format. Browser storage is origin-specific; export JSON before switching from a preview address to your domain.

## Connect Cloudflare Pages later

1. Create a Pages project and connect this GitHub repository. Grant the Cloudflare GitHub app access to this private repository.
2. Production branch: **`main`**.
3. Framework preset: **None**. Leave **Build command** empty.
4. Build output directory: **`public`**. Leave root directory at the repository root.
5. Deploy. Test the generated Pages URL and `/studio/`.
6. Add `ah-ray.com` in the Pages project's Custom domains settings and follow Cloudflare's DNS instructions. Studio then lives at `https://ah-ray.com/studio/`.

The browser binary is already committed. Cloudflare does not need to compile firmware. Domain/DNS changes are intentionally left for the owner.

Official guide: https://developers.cloudflare.com/pages/framework-guides/deploy-anything/

## Preview locally

```sh
python -m http.server 8978 --directory public --bind 127.0.0.1
```

Open `http://127.0.0.1:8978/` or `http://127.0.0.1:8978/studio/`. Use an HTTP server rather than opening HTML as a `file://` URL. A modern browser with WebAssembly and module workers is required.

## Update the shared FX package

The C++ source snapshot is in `fx/main`; bundled FastLED source and its license are in `fx/FastLED`; the host adapter is in `fx/host`. The build extracts rendering helpers directly from `led_engine.cpp` and virtual-node lookup directly from `node_config.cpp` to preserve firmware behavior.

Optional refresh from a local Dual S3 checkout:

```sh
python scripts/import-firmware.py /path/to/AH-RAY_NODE_DUAL_S3
```

Install **Zig 0.13.0** and Python 3. Then compile:

```sh
python scripts/build-wasm.py
```

If Zig is not on PATH, set the `ZIG` environment variable to its executable. The build writes `public/studio/fx.wasm` and `fx-manifest.json`; commit both alongside any updated sources. Source and binary SHA-256 hashes are recorded in the manifest. `Reload engine` in Studio resets the running browser engine; source edits require this build and a new website deployment.

## Verification

```sh
node --test tests/*.test.mjs
```

Compare the browser module with the native desktop renderer after an FX refresh (requires a local Dual S3 checkout and its existing emulator toolchain):

```sh
python scripts/native-fixtures.py /path/to/AH-RAY_NODE_DUAL_S3
node tests/parity.mjs
```

Only the `public` folder is published. The website previews effect RGB and coordinates; device calibration, electrical behavior, and radio timing remain hardware concerns.
