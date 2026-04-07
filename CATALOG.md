# Lovelang Catalog

## Core Runtime

- `include/love.h`: token and AST types shared by lexer/parser/runtime
- `src/lexer.c`: tokenization and keyword recognition
- `src/parser.c`: AST builder
- `src/runtime.c`: execution engine
- `src/main.c`: CLI and human-mode preprocessing

## Examples

- `examples/*.love`: sample Lovelang programs
- `examples/test/my.love`: quick smoke-style example

## Tooling

- `Makefile`: native build targets
- `run_love.sh`, `run_love.cmd`, `run_love.ps1`: helper runners

## VS Code Extension

- `extension/`: complete VS Code extension project
- `extension/CATALOG.md`: extension file catalog

## Docs and Licensing

- `README.md`: language documentation
- `LICENSE`: project license
