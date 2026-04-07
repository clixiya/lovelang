# lovelang-runtime

Cross-platform npm wrapper for the Lovelang interpreter.

YouTube: https://www.youtube.com/@clixiya

On install, this package detects your OS/CPU and downloads the matching Lovelang binary from GitHub releases.

If download is skipped during install (for example in CI), the `lovelang` command will auto-download the correct binary on first run.

## Install

```bash
npm install -g lovelang-runtime
```

Then run:

```bash
lovelang --help
lovelang ./examples/01-romantic-hello.love
```

From any folder:

```bash
lovelang /full/path/to/project/examples/01-romantic-hello.love
```

## How It Works

1. `postinstall` resolves your platform (`darwin`, `linux`, `win32`) and arch (`x64`, `arm64`).
2. It builds a GitHub release URL for the right asset.
3. It downloads the binary into `vendor/bin/` inside this package.
4. The `lovelang` bin script forwards your CLI args to that binary.
5. If the binary is missing at runtime, it is fetched automatically before executing your file.

## Expected Release Asset Names

- `lovelang-darwin-x64`
- `lovelang-darwin-arm64`
- `lovelang-linux-x64`
- `lovelang-linux-arm64`
- `lovelang-win32-x64.exe`
- `lovelang-win32-arm64.exe`

## Environment Variables

- `LOVELANG_GITHUB_OWNER` (default: `clixiya`)
- `LOVELANG_GITHUB_REPO` (default: `lovelang`)
- `LOVELANG_GITHUB_TAG` (default: `latest`)
- `LOVELANG_DOWNLOAD_BASE_URL` (optional)
: If set, installer uses `<base-url>/<asset-name>` directly.
- `LOVELANG_GITHUB_TOKEN` (optional)
: Added as `Authorization: Bearer <token>` for private/rate-limited downloads.
- `LOVELANG_SKIP_DOWNLOAD=1`
: Skips postinstall download.
- `LOVELANG_FORCE_DOWNLOAD=1`
: Re-downloads even if binary already exists.
- `LOVELANG_BIN_PATH` (runtime override)
: Use a custom local binary path when running `lovelang`.

## Development

```bash
npm test
npm run clean
```

## Notes

- This package is scaffolding for publishing/CI workflows.
- Release upload automation can be added in the next step when you ask to prepare for GitHub upload.
- For best user experience, keep GitHub release assets published for all supported targets listed above.
