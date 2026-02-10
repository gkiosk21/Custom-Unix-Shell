# Custom Unix Shell

A lightweight Unix shell implementation written in C, developed as part of the **HY345 - Operating Systems** course at the University of Crete.

**Author:** George Kiosklis (`csd5127`)

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Building](#building)
- [Usage](#usage)
  - [Running the Shell](#running-the-shell)
  - [Built-in Commands](#built-in-commands)
  - [I/O Redirection](#io-redirection)
  - [Pipelines](#pipelines)
  - [Shell Variables](#shell-variables)
  - [Control Flow](#control-flow)
  - [Command Chaining](#command-chaining)
- [Technical Details](#technical-details)
- [Limitations](#limitations)
- [Project Structure](#project-structure)
- [License](#license)

---

## Overview

`hy345sh` is a custom shell that replicates core functionality of traditional Unix shells like `bash` and `sh`. It reads user input from the terminal, parses commands, and executes them using standard POSIX system calls (`fork`, `exec`, `pipe`, `dup2`, `waitpid`). The shell supports external command execution, I/O redirection, multi-stage pipelines, shell variables with expansion, and control flow structures (`if/then/fi`, `for/do/done`).

---

## Features

| Feature | Description |
|---|---|
| **Command Execution** | Run any program available in `$PATH` via `fork`/`execvp` |
| **Built-in Commands** | `cd` (change directory) and `exit` (terminate the shell) |
| **I/O Redirection** | Input (`<`), output (`>`), and append (`>>`) redirection |
| **Pipelines** | Chain commands with `\|` (up to 32 stages) |
| **Shell Variables** | Assign (`VAR=value`) and expand (`$VAR`) variables |
| **If Statements** | Conditional execution: `if COND; then BODY; fi` |
| **For Loops** | Iteration: `for VAR in val1 val2 ...; do BODY; done` |
| **Command Chaining** | Execute multiple commands with `;` separators |
| **Multiline Input** | Automatic detection of incomplete control structures |
| **Nested Structures** | Support for nested `if` and `for` blocks |
| **Custom Prompt** | Displays `username@-5127-hy345sh:/current/path$` |

---

## Architecture

The shell follows a classic **read-eval-print loop (REPL)** architecture:

```
main()
 └── REPL loop
      ├── display_shell()        — print the prompt
      ├── fgets()                — read user input
      ├── read_multiline_cmd()   — handle incomplete control structures
      └── parse_and_exec()       — parse and dispatch commands
           ├── if_statement()    — handle if/then/fi
           ├── for_loop()        — handle for/in/do/done
           ├── pipelining()      — handle pipe chains (cmd1 | cmd2 | ...)
           └── execute_cmd()     — execute a single command
                ├── variable assignment (VAR=value)
                ├── var_expansion()  — expand $VAR references
                ├── cd / exit        — built-in commands
                └── fork + execvp    — external commands with I/O redirection
```

**Key modules:**

- **`display_shell()`** — Constructs the prompt using `getlogin()` and `getcwd()`.
- **`set_var()` / `get_Var()`** — Store and retrieve shell variables from an internal array.
- **`var_expansion()`** — Scans input strings and replaces `$VAR` tokens with their values.
- **`execute_cmd()`** — Handles variable assignments, I/O redirection parsing, built-in commands, and external command execution via `fork`/`execvp`.
- **`pipelining()`** — Splits a command line on `|`, creates pipes between stages, and forks child processes for each stage.
- **`if_statement()`** — Parses `if COND; then BODY; fi` syntax, evaluates the condition, and conditionally executes the body.
- **`for_loop()`** — Parses `for VAR in VALUES; do BODY; done`, iterates over values, and executes the body with the loop variable set.
- **`parse_and_exec()`** — Top-level parser that splits input on `;` (respecting quotes and control structure nesting), detects control flow keywords, and routes commands to the appropriate handler.

---

## Building

### Prerequisites

- GCC (or any C99-compatible compiler)
- POSIX-compliant OS (Linux, macOS)
- `make`

### Compile

```bash
make all
```

This produces the `hy345sh` executable in the project directory.

### Clean

```bash
make clean
```

Removes the compiled binary and object files.

---

## Usage

### Running the Shell

```bash
./hy345sh
```

On startup, the shell prints:

```
Shell initialized.
Welcome to my hy345shell...
Type 'exit' to terminate.
user@-5127-hy345sh:/your/current/directory$
```

### Built-in Commands

**`cd [directory]`** — Change the current working directory. With no argument, changes to `$HOME`.

```
cd /tmp
cd            # goes to $HOME
```

**`exit`** — Terminate the shell session.

```
exit
```

### I/O Redirection

Redirect standard input and output of commands:

```bash
# Redirect input from a file
cat < input.txt

# Redirect output to a file (overwrites)
ls -la > listing.txt

# Append output to a file
echo "new line" >> log.txt

# Combine input and output redirection
sort < unsorted.txt > sorted.txt
```

Embedded redirection (without spaces) is also supported:

```bash
cat<input.txt
ls>output.txt
echo "text">>file.txt
```

### Pipelines

Chain commands together so the output of one feeds into the input of the next:

```bash
ls -la | grep ".c"
cat file.txt | sort | uniq | wc -l
ps aux | grep ssh | head -5
```

Supports up to **32 pipeline stages**. Redirection can be combined with pipelines.

### Shell Variables

**Assign a variable:**

```bash
NAME=George
COUNT=42
GREETING="Hello World"
```

**Use a variable (expansion with `$`):**

```bash
echo $NAME
echo $GREETING
cd $HOME
```

Variables support alphanumeric characters and underscores in their names. Up to 128 variables can be stored.

### Control Flow

#### If Statements

```bash
# Single-line
if test 1 -eq 1; then echo "true"; fi

# Using a variable
x=5
if test $x -eq 5; then echo "x is five"; fi

# Multiline (the shell detects incomplete input and waits for 'fi')
if test -f myfile.txt
then
  echo "file exists"
fi
```

The condition is evaluated by running it as a command. If the command exits with status `0`, the body is executed.

#### For Loops

```bash
# Single-line
for i in 1 2 3 4 5; do echo $i; done

# Iterate over files
for f in a.txt b.txt c.txt; do cat $f; done

# Multiline
for color in red green blue
do
  echo $color
done
```

### Command Chaining

Execute multiple commands sequentially using `;`:

```bash
echo "first"; echo "second"; echo "third"
x=10; echo $x
mkdir test_dir; cd test_dir; ls
```

---

## Technical Details

| Constant | Value | Description |
|---|---|---|
| `MAX_LINE` | 4096 | Maximum input line length |
| `MAX_ARGS` | 128 | Maximum arguments per command |
| `MAX_VARS` | 128 | Maximum shell variables |
| `MAX_VAR_NAME` | 64 | Maximum variable name length |
| `MAX_VAR_VALUE` | 512 | Maximum variable value length |
| `MAX_PIPES` | 32 | Maximum pipeline stages |

- **Process management:** External commands are executed via `fork()` + `execvp()`. The parent waits for child completion with `waitpid()`.
- **Pipe implementation:** For N-stage pipelines, N-1 pipes are created. Each child process redirects its stdin/stdout using `dup2()`.
- **Redirection:** File descriptors are opened with `open()` and redirected using `dup2()` before `execvp()`.
- **Variable storage:** Variables are stored in a flat array of name-value pairs, searched linearly.
- **Multiline support:** When `if` or `for` is detected without its closing keyword (`fi`/`done`), the shell reads additional lines until the structure is complete, tracking nesting depth.

---

## Limitations

- No background process support (`&`)
- No signal handling (Ctrl+C terminates the shell)
- No command history or line editing (arrow keys)
- No glob/wildcard expansion (`*`, `?`)
- No `else`/`elif` branches in `if` statements
- No environment variable export to child processes
- Variable values are limited to 512 characters
- No quoted string support inside pipelines

---

## Project Structure

```
hy345sh/
├── hy345sh.c       # Shell implementation (single-file)
├── Makefile        # Build configuration
└── README.md       # Project documentation
```

---

## License

This project was developed for educational purposes as part of the HY345 Operating Systems course at the University of Crete.
