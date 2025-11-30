# M8-lang: esoteric runic program language that uses no whitespaces

### Features
No whitespaces, unreadable code, very esoteric

### M8 Language Grammar

- No whitespace is allowed anywhere in source programs.
- Statements are separated only by the single-character separator `ᛵ` or EOF.
- Types: primitive `i32` (`ᛰ`), `i64` (`ᛯ`), `bool` (`ᛨ`), `string` (`ꑭ`), plus user-defined struct types.
- Declarations: mutable variables (`ᚡ`), constants (`ᛍ`) format: `ᚡname᛬expr` and `ᛍname᛬expr`.
- Assignments and compound assignments.
- Expressions with precedence and comparison operators (`᛬᛬`, `ᛅ᛬`, `ᚲ`, `ᚲ᛬`) (some may be not implemented yet).
- Checked arithmetic operators using a prefix `ꑭ` (e.g., `ꑭ᛭`, `ꑭᛧ`, `ꑭ᛫`, `ꑭᛇ`) (not implemented yet).
- If/else without blocks; uses a single-character THEN separator `ᛜ` between condition and consequent.
- Return statement: `ᚷ` optionally followed by an expression.
- Integer, boolean, and string literals (strings use the quote rune `ᛌliteralᛌ` with C-style escapes `\n`, `\t`, `\r`, `\\`, `\0`, and `\ᛌ`).

### Functions

- Function declaration keyword: `ᚠ` for top-level functions, `ᛃ` for methods inside structs.
- General shape: `ᚠname᛬ᚮarg1᛬type1ᛵarg2᛬type2ᚭ᛬retType᛬body`.
- Parameters are separated by `ᛵ`, each as `name᛬type`.
- Return type is any valid `type` (primitive or struct).

### Structs with fields and methods

- Struct declaration keyword: `ᛋ`.
- General shape: `ᛋName᛬field1᛬type1ᛵfield2᛬type2ᛵ...ᛵᚠmethod᛬ᚮargsᚭ᛬retType᛬body`.
- Fields are `name᛬type` separated by `ᛵ`.
- Methods are declared with `ᚲ` inside the struct body and can access the struct's fields.

### Standard I/O functions

- `ᚱᚮᚭ` – input: returns a value based on the expected type of the surrounding expression. When assigned to `ꑭ` variables it reads a string, when assigned to `i32`, `i64`, or `bool` it reads the corresponding numeric value (`bool` reads integers and treats non-zero as true).
- `ᚹᚮexprᚭ` – output: evaluates `expr` and writes it with a trailing newline. Supports `i32`, `i64`, `bool`, and `string` expressions; bools print as `0/1` for now.

### Compile and run:
1. `cmake .`
2. `cd src`
3. `make`
4. `./M8-lang <source> <output>`

### How to test:
build, then run `cd .. && ctest`

### VS Code extension:
https://github.com/Tabatskyi/VSCodeM8LangSupport