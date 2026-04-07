# Lovelang

Lovelang is a C-based interpreted language with desi human-style phrases.

YouTube: https://www.youtube.com/@clixiya

This README documents the streamlined language surface currently enabled in this repo.

## Quick Start

```bash
npm install -g lovelang-runtime
lovelang --help
lovelang /full/path/to/lovelang/examples/01-romantic-hello.love
```

Build from source (optional):

```bash
git clone https://github.com/clixiya/lovelang
cd lovelang
make
./lovelang examples/01-romantic-hello.love
```

Debug helpers:

```bash
./lovelang examples/01-romantic-hello.love --tokens
./lovelang examples/01-romantic-hello.love --debug-love
./lovelang examples/01-romantic-hello.love --mode=shayari
```

## One-Minute Syntax

```love
yaad name hai "jaan"
vada promise hai "forever"

baby bolo naa "hello"
typing...

agar name barabar hai "jaan" toh
  bolo "welcome"
warna
  bolo "offline"
bas itna hi

love you baby byeee
```

## Core Model

- Mutable declarations: `yaad`, `yaad_karo`
- Immutable declaration: `vada`
- Output statements: `bolo`, `typing`
- Conditionals: `agar` / `bewafa` with `ye_karo` / `vo_karo`
- Loops: `jabtak`, `intezaar`
- Functions: `dhadkan`, `ehsaas`
- Blocks: `festival`
- Stop call: `love_byeee()` and alias `love_you_baby_byeee()`

Runtime value types:

- `int`
- `bool` (`sach`, `jhooth`)
- `string`
- `list`
- `map`
- `null`

## Human-Mode Aliases (Kept)

Preprocessor runs before lexing/parsing and normalizes these forms:

- `baby bolo na <expr>` -> `bolo <expr>`
- `baby bolo naa <expr>` -> `bolo <expr>`
- `baby_bolo_na <expr>` -> `bolo <expr>`
- `baby_bolo_naa <expr>` -> `bolo <expr>`
- `baby yad rakho <name ...>` -> `vada <name ...>`
- `baby yaad rakho <name ...>` -> `vada <name ...>`
- `baby_yad_rakho <name ...>` -> `vada <name ...>`
- `baby_yaad_rakho <name ...>` -> `vada <name ...>`
- `yaad karo <name ...>` -> `yaad_karo <name ...>`
- `pucho <prompt>` -> `dil_se_pucho(<prompt>)`
- `dil se pucho <prompt>` -> `dil_se_pucho(<prompt>)`
- `thoda ruko <ms>` -> `thoda_ruko(<ms>)`
- `typing...` -> `typing`
- `love you baby bye...` -> `love_byeee()`

Natural flow aliases are also kept:

- `agar <cond> toh` -> `agar (<cond>) ye_karo {`
- `bewafa <cond> toh` -> `bewafa (<cond>) ye_karo {`
- `jabtak <cond> tab tak` -> `jabtak (<cond>) ye_karo {`
- `intezaar <cond> tab tak` -> `intezaar (<cond>) ye_karo {`
- `warna` -> `} vo_karo {`
- `warna agar <cond> toh` -> else-if expansion
- `bas itna hi` -> closes natural blocks

One-liner form:

- `sun na agar <cond> toh <then_stmt> warna <else_stmt>`

## Declarations and Assignment

```love
yaad count = 0
yaad_karo score hai 10
vada badge hai "gold"

count hai count milan 1
```

Notes:

- `vada` is immutable.
- Reassigning a `vada` variable is a runtime error.
- `name hai expr` is accepted as assignment when `name` is not a reserved keyword.

## Output

```love
bolo "hello"
bolo 42
typing...
```

`typing` prints a typing indicator line.

## Input, Time, and Utility Built-ins

Lovelang includes interactive and utility built-ins:

- `dil_se_pucho(prompt?)`:
  - reads one line from stdin
  - returns string
  - alias: `input(prompt?)`
- `kismat(min, max)`:
  - random integer in inclusive range
- `lafz_len(value)`:
  - string length of value (after text conversion)
- `lambai(value)` / `len(value)`:
  - length for `string`, `list`, and `map`
- `abhi_time()`:
  - current unix timestamp (seconds)
- `thoda_ruko(ms)`:
  - pause execution for milliseconds

Example:

```love
bolo "naam batao"
yaad name hai dil_se_pucho("> ")
yaad lucky hai kismat(1, 100)
yaad chars hai lambai(name)
yaad now hai abhi_time()

bolo "hey " milan name
bolo "lucky: " milan lucky
bolo "length: " milan chars
bolo "time: " milan now

thoda_ruko(700)
bolo "done"
```

## Collections (List and Map)

List built-ins:

- `list_nayi()` / `pyaar_list()` -> creates empty list
- `list_daal(list, value)` / `pyaar_daal(list, value)` -> append item, returns new length
- `list_nikaal(list)` -> pops last item (errors on empty list)
- `list_lao(list, index)` -> reads item at index (errors if out of range)
- `list_set(list, index, value)` -> updates item at index, returns `sach`/`jhooth`

Map built-ins:

- `map_naya()` / `raaz_map()` -> creates empty map
- `map_set(map, key, value)` -> sets key, returns `sach`
- `map_get(map, key)` -> returns value or `null`
- `map_has(map, key)` -> returns `sach`/`jhooth`
- `map_keys(map)` -> returns list of keys

Notes:

- map keys are converted to string internally.
- printing list/map values uses readable forms like `[a, b]` and `{key: value}`.

## String Toolkit

- `lafz_trim(value)`
- `lafz_lower(value)`
- `lafz_upper(value)`
- `lafz_contains(text, needle)`
- `lafz_replace(text, from, to)`
- `lafz_split(text, sep)` -> list
- `lafz_join(list, sep)` -> string

Example:

```love
yaad names hai list_nayi()
list_daal(names, "clixiya")
list_daal(names, "rani")
bolo lafz_join(names, ", ")

yaad text hai "   PyAr Se  "
bolo lafz_trim(text)
bolo lafz_lower(text)
bolo lafz_upper(text)
bolo lafz_replace("i love code", "love", "adore")
```

## Type and Conversion Helpers

- `type_of(value)` / `kya_type(value)` -> `int`, `bool`, `string`, `list`, `map`, or `null`
- `to_text(value)` / `text_banao(value)` -> string form
- `to_int(value)` / `int_banao(value)` -> integer form
- `to_bool(value)` / `bool_banao(value)` -> truthy/falsey conversion

## Modules (`import` / `export`)

Modules are preprocessed before lex/parse:

- `import "path/to/file.love"` inlines the target source.
- relative paths resolve from the importing file location.
- each file is imported only once per run (deduped).
- `export` is accepted and stripped (useful for readable module files).

Example:

```love
import "modules/romantic_utils.love"

bolo greet("Clixiya")
bolo greet("Clixiya", mood = "shayari", punct = "...")
```

## Filesystem Built-ins

- `dil_khol_ke_padho(path)` / `file_padho(path)` -> read full file text
- `ishq_likhdo(path, text)` / `file_likho(path, text)` -> overwrite file, returns `sach`/`jhooth`
- `ishq_joddo(path, text)` / `file_jodo(path, text)` -> append file, returns `sach`/`jhooth`
- `raasta_hai_kya(path)` / `file_hai_kya(path)` -> existence check

Example:

```love
yaad note_file hai "examples/test/module_note.txt"
ishq_likhdo(note_file, "line1")
ishq_joddo(note_file, "\nline2")
bolo raasta_hai_kya(note_file)
bolo dil_khol_ke_padho(note_file)
```

## Conditionals

Core style:

```love
agar (count > 0) ye_karo {
  bolo "positive";
} vo_karo {
  bolo "zero or negative";
}
```

Natural style:

```love
agar count bada hai 0 toh
  bolo "positive"
warna
  bolo "zero or negative"
bas itna hi
```

## Loops

```love
yaad i hai 0

intezaar i chhota hai 3 tab tak
  bolo i
  i hai i milan 1
bas itna hi
```

Loop guard triggers after 1,000,000 iterations.

## Functions

```love
dhadkan greet(name, mood = "romantic", punct = "!") {
  ehsaas mood milan ": " milan name milan punct
}

bolo greet("baby")
bolo greet("baby", mood = "shayari")
bolo greet("baby", mood = "toxic", punct = "...")
```

`ehsaas` is valid only inside `dhadkan`.

Notes:

- function params can have defaults (`name = expr` or `name hai expr`).
- calls support named arguments (`fn(x = 1, y = 2)`).
- unknown named args or too many positional args produce runtime errors.

## Festival Blocks

```love
festival diwali {
  bolo "lights";
}
```

At runtime it prints `festival mode: <name>` and executes the block.

## Stop Behavior

Supported:

- `love_byeee()`
- `love_you_baby_byeee()`
- human phrase `love you baby bye...`

Behavior:

- prints farewell (mode-based tone)
- stops execution of remaining statements

## Operators

Symbol operators:

- Arithmetic: `+ - * / %`
- Compare: `== != < <= > >=`
- Logical: `&& || !`

Word operators:

- Arithmetic: `milan`, `doori`, `intense`, `divide`
- Compare: `barabar hai`, `same hai`, `equal hai`, `alag hai`, `chhota hai`, `bada hai`, `chhota ya barabar hai`, `bada ya barabar hai`
- Logical: `aur`, `ya`, `nahi`

## Comments

```love
// line comment
# line comment
~~ line comment
baby ignore karo natural comment
/* block comment */
```

## What Was Removed (Streamlined Mode)

These are intentionally not part of the current syntax:

- old mutable keywords: `dil`, `mood`, `junoon`
- old const keyword: `ishq`
- old output family: `babu_miss_you`, `shayarana`, `status`, `last_seen`, `seen`, `auto_reply`, `gaana`
- removed built-ins: `chai_break`, `date_plan`

If these appear in source now, parser reports an unknown statement.

## CLI

```text
lovelang <file.love> [--tokens] [--mode romantic|toxic|shayari] [--debug-love]
```

- `--tokens`: print token stream after preprocessing
- `--mode`: output tone mode
- `--debug-love`: print variable set/update logs

## Project Layout

- `include/love.h`: token/AST declarations
- `src/main.c`: CLI and preprocessor
- `src/lexer.c`: tokenizer
- `src/parser.c`: parser
- `src/runtime.c`: execution engine
- `examples/*.love`: runnable programs
- `examples/modules/*.love`: module import demos
- `extension/`: VS Code syntax + snippets + hovers + run command + settings webview
- `CATALOG.md`: quick file catalog for the project
- `LICENSE`: project license

## FAQ

Q: Is this still interpreted?

A: Yes. Pipeline is: file read -> preprocess (human phrase normalization) -> lex -> parse -> runtime execute.

Q: Can I mix natural and core syntax?

A: Yes, that is the intended workflow.

Q: How do modules work?

A: Use `import "path.love"`. Imports are resolved relative to the current file and each imported file is included once. `export` is optional and helps mark reusable declarations.

Q: Does `love you baby byeee` stop execution?

A: Yes. It normalizes to `love_byeee()` and sets runtime stop flag.