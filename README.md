Mini Shell
==========

This project is a custom Unix-like shell written in C++. It demonstrates core process and memory management concepts by parsing user input, executing commands, and managing system resources like file descriptors and child processes.

Features
--------

This shell implements a wide range of features found in modern POSIX-compliant shells:

-   **Robust Parsing:** A hand-written parser that understands a complex shell grammar and gracefully handles errors like malformed pipes (`||`), missing filenames (`>` ), and pipes at the start or end of a command.

-   **Process Management:** Correctly uses `fork()`, `execvp()`, and `waitpid()` to create and manage child processes for external commands.

-   **Built-in Commands:**

    -   `cd [dir]`: Changes the current working directory. Defaults to `$HOME` if no directory is provided.

    -   `exit`: Terminates the Mini Shell.

-   **Pipelines (`|`):**

    -   Supports chaining multiple commands, where the `stdout` of one command is piped to the `stdin` of the next.

    -   Example: `ls -l | grep .cc | wc -l`

-   **Full File Redirection:**

    -   `>`: Redirect `stdout` (Standard Output) to a file, overwriting its contents.

    -   `>>`: Redirect `stdout` to a file, appending to the end of it.

    -   `<`: Redirect `stdin` (Standard Input) to come from a file.

    -   `2>`: Redirect `stderr` (Standard Error) to a file.

    -   `>>&`: Redirect *both* `stdout` and `stderr` to a file, appending to it.

-   **Background Processes (`&`):**

    -   Allows a command to run in the background, freeing the shell to accept new commands immediately.

-   **Process Termination Logging:**

    -   A `SIGCHLD` signal handler correctly "reaps" all terminated child processes (both foreground and background) to prevent zombies.

    -   Logs every terminated child process to `log.txt`.

-   **Bonus: Wildcard Expansion (`*`, `?`):**

    -   Automatically expands arguments containing wildcards into a list of matching filenames using `glob()`.

    -   Matches `csh` behavior: if no files match a pattern, the original pattern is used as the argument.

Project Architecture
--------------------

-   **`Makefile`**: Contains all rules for compiling and linking the project.

-   **`command.h` / `command.cc`**: Defines the `Command` and `SimpleCommand` data structures. Contains all logic for parsing (`parse`), execution (`execute`), built-ins, pipes, redirection, and signal handling.

-   **`tokenizer.h` / `tokenizer.cc`**: A lexical analyzer that converts the raw input string into a list of tokens (e.g., `TOKEN_COMMAND`, `TOKEN_PIPE`).

How to Build
------------

A `Makefile` is provided for easy compilation.

**Prerequisites:** You must have `g++` (C++17 compliant) and `make` installed on your system.

To build the executable, clone the repository and run `make`:

```
# 1. Clone your repository (example)
# git clone ...

# 2. Navigate into the directory
cd your-project-directory

# 3. Compile the project
make

```

This will compile all `.cc` source files and create a single executable file named **`myshell`**.

How to Run
----------

After building, you can run your shell from the terminal:

```
./myshell

```

You will be greeted by the `myshell>` prompt.

Demonstration of Features
-------------------------

Here are examples of commands you can run in `myshell`:

### Basic Commands

```
# Run a simple command
myshell> ls -al

# Use built-in commands
myshell> pwd
myshell> cd ..
myshell> pwd

```

### File Redirection

```
# Create a new file with text
myshell> echo "Hello World" > output.txt

# Append to that file
myshell> echo "Second Line" >> output.txt

# Use that file as input
myshell> cat < output.txt
Hello World
Second Line

# Redirect an error (this file does not exist)
myshell> cat non_existent_file 2> error.log

# Check the error log
myshell> cat error.log
myshell: cat: non_existent_file: No such file or directory

```

### Pipelines

```
# Count the number of .cc files
myshell> ls -l | grep .cc | wc -l
2

```

### Wildcard Expansion

```
# 'echo' will receive the expanded list of files
myshell> echo *.cc
command.cc tokenizer.cc

# If no files match, the original pattern is used
myshell> echo *.nonexistent
*.nonexistent

```

### Background Processes & Logging

```
# Run a 2-second process in the background
myshell> sleep 2 &

# The prompt returns immediately. You can run other commands.
myshell> echo "I am not blocked!"
I am not blocked!

# After 2 seconds, the SIGCHLD handler will log the termination.
# Check the log file:
myshell> cat log.txt
Child process 12345 terminated.

```

Cleaning Up
-----------

To remove all compiled object files (`.o`) and the final `myshell` executable, run:

```
make clean

```
