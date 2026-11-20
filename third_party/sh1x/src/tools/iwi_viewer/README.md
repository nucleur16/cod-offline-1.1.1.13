# sh1x IWI Viewer

Standalone native Windows image viewer for Call of Duty Online Closed Alpha.
Build output: `dist/sh1x-IWI-Viewer.exe`. Python is bundled into the EXE;
the viewing computer does not need Python or a web browser.

The default folder is
`D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13\main\_decrypted`.
Use **File > Open folder** (Ctrl+O) to index all images recursively, including
every nested subfolder. Search matches the relative path as well as the filename.
Open individual files with Ctrl+Shift+O, or drag a folder/image onto the window.

Select an image, then **Export PNG** (Ctrl+S) to choose its destination.
Export preserves full resolution and the original alpha channel regardless of
the preview zoom/channel selection. Existing destinations use the native save
dialog's overwrite confirmation. Source files are only read.

Mouse wheel zooms; drag pans. Fit, 100%, 200%, 400%, nearest-pixel rendering,
color-only preview and alpha-mask preview are available.

Supported: CoDOL v9 IWI (BGRA8, BGR8, LA8, L8, A8, DXT1, DXT3, DXT5), PNG,
JPEG, DDS, TGA, BMP, GIF, WebP, TIFF, ICO, PCX and portable bitmap formats.
Animated/multipage images show and export their first frame. Unsupported or
damaged images display a decoding error. Other games' IWI versions and wavelet
formats are not supported. Cube/slice handling follows the reference layout;
no actual C1 cubemap was available for verification.

## Build and test

Run `powershell -ExecutionPolicy Bypass -File build.ps1` from this folder.
The build script isolates dependency lookup from unrelated tool PATH entries
and bundles Qt's matching VC runtime; use it for reproducible EXE packaging.
Run `.venv\Scripts\python.exe -m unittest test_viewer -v` for fixture tests
and a native Qt window/PNG export smoke test. Set `QT_QPA_PLATFORM=windows`
for the test to render with Windows fonts instead of the offscreen backend.

Verified decoder with 75 actual C1 textures across six formats. Tests cover
raw channel ordering/alpha, DXT1/3/5 known-color blocks, truncation, primary mip
selection, recursive mixed-format discovery and pixel-exact PNG export through
the native window. C1 files establish the version 9 header and mip layout;
OpenBLOPS `Engine/Renderer/gfx_d3d/r_image_load_obj.cpp` was consulted for format
semantics. This tool does not interact with the game, lobby or server processes.
