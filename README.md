# M8-lang: esoteric runic program language that uses no whitespaces

### Features
No whitespaces, unreadable code, very esoteric

### M8 Language Grammar

- No whitespace is allowed anywhere in source programs.
- Statements are separated only by the single-character separator 'ᛵ' or EOF.
- Types: i32 ᛰ, i64 ᛯ, bool ᛨ
- Declarations: mutable variables (ᚡ), constants (ᛍ) format: ᚡname᛬expr and ᛍname᛬expr
- Assignments and compound assignments
- Expressions with precedence and comparison operators (᛬᛬ ᛅ᛬, ᚲ, ᚲ᛬) (not implemented yet)
- Checked arithmetic operators using a prefix ꑭ (e.g., ꑭ᛭, ꑭᛧ, ꑭ᛫, ꑭᛇ) (not implemented yet)
- If/else without blocks; use a single-character THEN separator ᛜ between condition and consequent
- Return statement: ᚷ optionally followed by an expression
- Statement separation: only by 'ᛵ'
- Integer and boolean literals

### Compile and run:
1. `cmake .`
2. `cd src`
3. `make`
4. `./M8-lang <source> <output>`

### How to test:
build, then run `cd .. && ctest`

### VS Code extension:
https://github.com/Tabatskyi/VSCodeM8LangSupport