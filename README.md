[![progress-banner](https://backend.codecrafters.io/progress/shell/2aeafc99-dc13-43cc-9655-01de3f92833a)](https://app.codecrafters.io/users/SameeraJuman?r=2qF)

This is a starting point for C solutions to the
["Build Your Own Shell" Challenge](https://app.codecrafters.io/courses/shell/overview).

In this challenge, you'll build your own POSIX compliant shell that's capable of
interpreting shell commands, running external programs and builtin commands like
cd, pwd, echo and more. Along the way, you'll learn about shell command parsing,
REPLs, builtin commands, and more.

**Note**: If you're viewing this repo on GitHub, head over to
[codecrafters.io](https://codecrafters.io) to try the challenge.

# Passing the first stage

The entry point for your `shell` implementation is in `src/main.c`. Study and
uncomment the relevant code, then run the command below to execute the tests on
our servers:

```sh
codecrafters submit
```

Time to move on to the next stage!

# Stage 2 & beyond

Note: This section is for stages 2 and beyond.

1. Ensure you have `cmake` installed locally
1. Run `./your_program.sh` to run your program, which is implemented in
   `src/main.c`.
1. Run `codecrafters submit` to submit your solution to CodeCrafters. Test
   output will be streamed to your terminal.

# My Implementation
The main code is in `src/main.c`.

### Completed extensions
 
**Base**
- Print a prompt, handle invalid commands, REPL loop
- `exit`, `echo`, `type`, `pwd`, `cd` builtins
- Locate and run external programs via `PATH`

**Navigation**
- `cd` with absolute paths, relative paths, and `~` (home directory)

**Quoting**
- Single quotes, double quotes, backslash escaping inside and outside quotes
- Executing quoted executables

**Redirection**
- Redirect stdout (`>`, `1>`) and stderr (`2>`)
- Append stdout (`>>`, `1>>`) and stderr (`2>>`)

**Command Completion**
- TAB completion for builtins and executables
- Completion with arguments
- Bell on no match
- Multiple match display (alphabetically sorted)
- Partial completion to longest common prefix

**Filename Completion**
- File and directory completion
- Nested path completion (e.g. `foo/bar/file`)
- Trailing `/` appended for directories
- Multiple matches with partial completion
- Multi-argument tab completion

**Programmable Completion**
- `complete -C <script> <cmd>` to register a completer script
- `complete -p <cmd>` to display registered specification
- `complete -r <cmd>` to unregister a completion
- Forks completer script with `COMP_LINE` / `COMP_POINT` env vars
- Passes command-line arguments to completer
- Multiple completer candidates, longest common prefix
- Handles missing specifications

**Background Jobs**
- `jobs` builtin with job numbers, status, and markers (`+`, `-`)
- Starting background jobs with `&`
- Background job output printing
- Reaping completed jobs (single and multiple)
- Reaping before the next prompt
- Recycling job numbers

**Pipelines**
- Dual-command pipelines (`cmd1 | cmd2`)
- Pipelines with builtins
- Multi-command pipelines (`cmd1 | cmd2 | cmd3 | ...`)
---
 
### Not implemented
 
- History (builtin, listing, limiting, arrow navigation, persistence)
- Parameter Expansion (`declare`, shell variables, `$VAR` / `${VAR}` expansion)
