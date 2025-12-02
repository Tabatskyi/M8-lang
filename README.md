# M8-lang: esoteric runic program language that uses no whitespaces

### Features
No whitespaces, unreadable code, very esoteric, generic programming with templates

### M8 Language Grammar

- No whitespace is allowed anywhere in source programs.
- Statements are separated only by the single-character separator `ᛵ` or EOF.
- Types: primitive `i32` (`ᛰ`), `i64` (`ᛯ`), `bool` (`ᛨ`), `string` (`ꑭ`), template type (`ᛸ`), plus user-defined struct types.
- Declarations: mutable variables (`ᚡ`), constants (`ᛍ`) format: `ᚡname᛬expr` and `ᛍname᛬expr`.
- Assignments and compound assignments (`᛬`, `᛭᛬`, `ᛧ᛬`, `᛫᛬`, `ᛇ᛬`).
- Expressions with precedence and comparison operators (`᛬᛬`, `ᛅ᛬`, `ᚲ`, `ᚲ᛬`).
- Logical/bitwise operators: AND (`ᛤ`), OR (`ᚢ`), XOR (`ᛡ`) - work as logical operators on booleans, bitwise on integers.
- Logical operators: AND (`ᛤ`), OR (`ᚢ`), XOR (`ᛡ`) for boolean expressions.
- If/else statements: `ᛗ` for if, `ᛎ` for else; uses a single-character THEN separator `ᛜ` between condition and consequent.
- Return statement: `ᚷ` optionally followed by an expression.
- Integer, boolean, and string literals (strings use the quote rune `ᛌliteralᛌ` with C-style escapes `\n`, `\t`, `\r`, `\\`, `\0`, and `\ᛌ`).

### Functions

- Function declaration keyword: `ᚠ` for top-level functions, `ᛃ` for methods inside structs.
- General shape: `ᚠname᛬ᚮarg1᛬type1ᛵarg2᛬type2ᚭ᛬retType᛬body`.
- Parameters are separated by `ᛵ`, each as `name᛬type`.
- Return type is any valid `type` (primitive, struct, or template parameter `ᛸ`).

### Logical and Bitwise Operators

- `ᛤ` (AND): Logical AND for booleans, bitwise AND for integers
- `ᚢ` (OR): Logical OR for booleans, bitwise OR for integers
- `ᛡ` (XOR): Logical XOR for booleans, bitwise XOR for integers
- `ᛅ` (NOT): Logical NOT (unary negation)
- Operator precedence: OR < XOR < AND < Equality < Arithmetic
- Examples:
  - Boolean: `ᛉᛤᛣ` (true AND false → false)
  - Bitwise: `8ᛰᚢ4ᛰ` (8 OR 4 → 12 in i32)

### Template Functions

- Functions can be generic using the template type parameter `ᛸ`.
- Template functions are declared with `ᛸ` as parameter or return type.
- Template instantiation is automatic upon first call with concrete types.
- The compiler deduces template arguments from call site argument types.
- Example: `ᚠidentity᛬ᚮx᛬ᛸᚭ᛬ᛸ᛬ᚮᚷx᛬ᚭ` creates a generic identity function.
- Each unique combination of concrete types generates a separate instantiation.

### Structs with fields and methods

- Struct declaration keyword: `ᛋ`.
- General shape: `ᛋName᛬field1᛬type1ᛵfield2᛬type2ᛵ...ᛵᛃmethod᛬ᚮargsᚭ᛬retType᛬body`.
- Fields are `name᛬type` separated by `ᛵ`.
- Methods are declared with `ᛃ` inside the struct body and can access the struct's fields.
- Struct literals: `TypeName᛬ᚮarg1ᛵarg2ᛵ...ᚭ` initializes fields in order.
- Field access and member function calls are supported with chaining.

### Standard I/O functions

- `ᚱᚮᚭ` – input: returns a value based on the expected type of the surrounding expression. When assigned to `ꑭ` variables it reads a string, when assigned to `i32`, `i64`, or `bool` it reads the corresponding numeric value (`bool` reads integers and treats non-zero as true).
- `ᚹᚮexprᚭ` – output: evaluates `expr` and writes it with a trailing newline. Supports `i32`, `i64`, `bool`, and `string` expressions; bools print as `0/1` for now.

### Compiler Architecture

**Lexer**: Tokenizes runic input with no whitespace handling.

**Parser**: Builds Abstract Syntax Tree (AST) from tokens following M8 grammar.

**Semantic Analyzer**: 
- Type checking and inference
- Symbol resolution with scoped symbol tables
- Template function instantiation via AST cloning
- Error reporting with line numbers

**AST Cloner**: 
- Template-based generic cloning system using C++ templates
- `template<typename T> std::unique_ptr<T> clone(const T& node)` - compile-time type-safe node cloning
- `template<typename T> std::vector<std::unique_ptr<T>> cloneList(...)` - generic list cloning
- Uses `if constexpr` for zero-overhead compile-time dispatch
- Supports type substitution for template instantiation
- Preserves scope information through remapping
- Deep copies entire AST subtrees while substituting template parameters

**Code Generator**: Emits LLVM IR for compilation to native code.

### Build Instructions

```bash
cmake .
cmake --build build
```

### Usage

```bash
./build/src/M8-lang <source.m8> <output.ll>
```

The compiler generates LLVM IR output that can be compiled to executable with `clang` or `llc`.

### Testing

```bash
ctest --test-dir build
```

Or run the end-to-end test script:
```bash
python3 scripts/run_e2e_test.py
```

### VS Code extension:
https://github.com/Tabatskyi/VSCodeM8LangSupport