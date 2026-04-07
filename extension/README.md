# Love Lang Tools (VS Code)

Lovelang extension for `.love` files with:

YouTube: https://www.youtube.com/@clixiya

- syntax highlighting (TextMate + fallback decorations)
- IntelliSense + snippets
- hover docs
- run command from editor title and command palette
- visual webview settings panel
- support for latest language-core features (modules, collections, string toolkit, filesystem built-ins)

## Features

- Language ID: `lovelang`
- File extension: `.love`
- Grammar support for:
  - declarations and assignments
  - operators (symbol + word forms)
  - conditionals/loops/functions/festival blocks
  - module keywords: `import`, `export`
  - function defaults + named-argument call style
  - human-mode phrases
  - built-ins:
    - input/time: `dil_se_pucho`/`input`, `kismat`, `lafz_len`, `lambai`/`len`, `abhi_time`, `thoda_ruko`
    - type/convert: `type_of`/`kya_type`, `to_text`/`text_banao`, `to_int`/`int_banao`, `to_bool`/`bool_banao`
    - list/map: `list_nayi`/`pyaar_list`, `list_daal`/`pyaar_daal`, `list_nikaal`, `list_lao`, `list_set`, `map_naya`/`raaz_map`, `map_set`, `map_get`, `map_has`, `map_keys`
    - string toolkit: `lafz_trim`, `lafz_lower`, `lafz_upper`, `lafz_contains`, `lafz_replace`, `lafz_split`, `lafz_join`
    - filesystem: `dil_khol_ke_padho`/`file_padho`, `ishq_likhdo`/`file_likho`, `ishq_joddo`/`file_jodo`, `raasta_hai_kya`/`file_hai_kya`
  - phrase aliases: `pucho`, `dil se pucho`, `thoda ruko`
- Auto-association:
  - `.love` files are auto-switched to Lovelang mode on open
- Fallback decoration highlighting:
  - if your theme does not color custom scopes, extension still colors keywords/operators/functions/events and expanded built-in groups

Language examples to try quickly:

- `../examples/11-collections-and-strings.love`
- `../examples/12-modules-and-filesystem.love`
- `../examples/16-all-features-showcase.love`

## Commands

- `Lovelang: Run Current File` (`loveLanguage.runCurrentFile`)
- `Lovelang: Settings` (`loveLanguage.openSettingsPanel`)
- `Lovelang: Open README` (`loveLanguage.openExtensionReadme`)

Editor title actions are shown for `.love` files:

- Run
- Settings

## Run Command Behavior

`Lovelang: Run Current File` builds command from settings and executes in terminal.

Priority:

1. If `runUseScript=true`, uses run script (`run_love.sh` on macOS/Linux, `run_love.cmd` on Windows) unless overridden by `runScriptPath`.
2. If script is missing/disabled, falls back to `runBinaryPath`.

Optional flags are appended from settings:

- `--mode` via `runMode`
- `--tokens` via `runTokens`
- `--debug-love` via `runDebugLove`
- extra args via `runExtraArgs`

## Settings (All Options)

- `lovelang.runUseScript` (bool)
- `lovelang.runScriptPath` (string)
- `lovelang.runBinaryPath` (string)
- `lovelang.runMode` (string)
- `lovelang.runTokens` (bool)
- `lovelang.runDebugLove` (bool)
- `lovelang.runExtraArgs` (string[])
- `lovelang.runInNewTerminal` (bool)
- `lovelang.suggestions.includeHumanPhrases` (bool)
- `lovelang.autoAssociateLoveFiles` (bool)
- `lovelang.forceDecorationHighlighting` (bool)

You can edit these either in VS Code Settings or in `Lovelang: Settings` webview.

## Build and Install

```bash
cd extension
npm run check
npm run package
```

Install generated VSIX:

```bash
code --install-extension love-lang-tools-<version>.vsix --force
```

Profile install example:

```bash
code --install-extension love-lang-tools-<version>.vsix --force --profile comp
```

## Troubleshooting

If highlighting still looks plain:

1. Run `Developer: Reload Window`.
2. Confirm language mode is `Lovelang`.
3. Keep `lovelang.forceDecorationHighlighting = true`.

## Files

See [CATALOG.md](CATALOG.md) for extension file catalog.
