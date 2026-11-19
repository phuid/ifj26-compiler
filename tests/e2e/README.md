# End-to-end tests

Whole-chain tests: IFJ26 source → `ifj26-compiler` → IFJcode26 → `ic26int` → stdout and exit code.

```bash
make e2e                                  # build, then run every case
make e2e E2E_ARGS="-v -k basic"           # one group, show diffs
IC26INT=/path/to/ic26int tests/e2e/run_e2e.sh
```

The compiler reads the source on stdin, writes IFJcode26 on stdout and exits with the error code from
the specification. Without `ic26int` on `PATH` (or in `IC26INT`), cases that need the interpreter are
only compiled and reported as `SKIP`; compile-error cases still run.

## Case files

`cases/<group>/<name>.ifj26` is the program. Next to it:

| File | Meaning |
|---|---|
| `<name>.rc` | expected exit code; missing means `0`. `1`–`10`, `99` = compiler error, the interpreter does not run. `40`, `57`, `58` = interpreter exit code. |
| `<name>.out` | expected stdout of the interpreter, compared byte for byte; missing means empty |
| `<name>.in` | stdin for the interpreter; missing means empty |

## Groups

| Group | Content |
|---|---|
| `basic` | smallest programs, literals, comments, layout |
| `variables` | `val`/`var`, inference, shadowing, scopes |
| `expressions` | priority, associativity, Int division toward zero, Double, strings, relations, division by zero (57) |
| `control` | `if`/`else`, `while`, nesting, `return` from a loop |
| `functions` | calls, inferred and `Unit` return types, `return`, recursion |
| `builtins` | `read*`, `toStr`, `toInt`, `strLength`, `subStr` (40), `ord`, `chr` (58) |
| `programs` | larger complete programs |
| `errors` | compile errors 1–7, one defect per program |
| `errors_noext` | errors **only without extensions** (FUNEXP, BOOLTHEN, CONTROL); skip this group if the team implements them |

Valid programs use only the base language: call arguments are terms (literals or variables), so
intermediate results are stored in a `val` first.

## Cases to confirm

The expected result of these follows the specification, but the wording leaves room; confirm on the
forum or with `ic26int`:

- `errors/syn_09_unexpected_indent` — deeper indentation inside a block: 2, not 1.
- `errors/syn_17_chained_relational` — `1 < 2 < 3`: 2 (no associativity), not 6.
- `errors/syn_20_block_comment_then_token` — token after a multi-line block comment: 2, not 1.
- `errors/sem3_07_self_reference_in_init` — `val x = x + 1`: 3.
- `errors/sem4_05_redefine_builtin` — user `def print`: 4.
- `errors_noext/05_negative_literal_argument` — `f(-1)` is an expression, not a term.
- `errors_noext/06_store_relational_result` — `val b = 1 < 2`: 6.
