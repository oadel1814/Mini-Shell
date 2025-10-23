#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <glob.h>
#include <fcntl.h>
#include <wordexp.h>
#include <vector>
#include <string>
#include <cctype>
#include <iostream>
#include <sstream>
#include "command.h"
#include "tokenizer.h"

Command Command::_currentCommand;

void expand_wildcards(SimpleCommand *currentSimpleCommand, char *arg_string) {

    if (strchr(arg_string, '*') == NULL && strchr(arg_string, '?') == NULL) {
        currentSimpleCommand->insertArgument(arg_string);
        return;
    }

    // Wildcards were found
    glob_t glob_results;
    int glob_status = glob(arg_string, GLOB_NOCHECK, NULL, &glob_results);

    if (glob_status == 0) {
        // the pattern was found
        // we need to iterate over the results and insert them
        for (size_t i = 0; i < glob_results.gl_pathc; i++) {
            currentSimpleCommand->insertArgument(strdup(glob_results.gl_pathv[i]));
        }
    } else {
        // glob failed.
        currentSimpleCommand->insertArgument(arg_string);
    }

    globfree(&glob_results);

    if (glob_status == 0) {
        free(arg_string);
    }
}

bool parse(std::vector<Token> &tokens)
{
    Command::_currentCommand.clear();
    SimpleCommand *currentSimpleCommand = new SimpleCommand();

    TokenType lastTokenType = TOKEN_EOF;
    bool hasCommand = false;

    // Error Checking: Pipe at start
    if (!tokens.empty() && tokens[0].type == TOKEN_PIPE) {
        fprintf(stderr, "Error: Malformed pipe at start.\n");
        delete currentSimpleCommand;
        return false;
    }

    for (size_t i = 0; i < tokens.size(); i++) {
        Token &token = tokens[i];

        if (token.type == TOKEN_COMMAND) {
            hasCommand = true;
            expand_wildcards(currentSimpleCommand, strdup(token.value.data()));
        }
        else if (token.type == TOKEN_ARGUMENT) {
            expand_wildcards(currentSimpleCommand, strdup(token.value.data()));
        }
        else if (token.type == TOKEN_PIPE) {
            if (lastTokenType == TOKEN_PIPE) {
                fprintf(stderr, "Error: Malformed consecutive pipes.\n");
                delete currentSimpleCommand;
                return false;
            }
            if (currentSimpleCommand->_numberOfArguments == 0) {
                 fprintf(stderr, "Error: Malformed pipe with no command.\n");
                 delete currentSimpleCommand;
                 return false;
            }
            Command::_currentCommand.insertSimpleCommand(currentSimpleCommand);
            currentSimpleCommand = new SimpleCommand();
        }
        else if (token.type == TOKEN_REDIRECT) { // >
            if (i + 1 < tokens.size()) {
                TokenType next_type = tokens[i + 1].type;
                if (next_type == TOKEN_COMMAND || next_type == TOKEN_ARGUMENT) {
                    Command::_currentCommand._outFile = strdup(tokens[i + 1].value.data());
                    Command::_currentCommand._append = false;
                    i++;
                } else {
                    fprintf(stderr, "Error: Expected filename after >\n");
                    delete currentSimpleCommand;
                    return false;
                }
            } else {
                fprintf(stderr, "Error: Missing filename for >\n");
                delete currentSimpleCommand;
                return false;
            }
        }
        else if (token.type == TOKEN_APPEND) { // >>
            if (i + 1 < tokens.size()) {
                TokenType next_type = tokens[i + 1].type;
                if (next_type == TOKEN_COMMAND || next_type == TOKEN_ARGUMENT) {
                    Command::_currentCommand._outFile = strdup(tokens[i + 1].value.data());
                    Command::_currentCommand._append = true;
                    i++;
                } else {
                    fprintf(stderr, "Error: Expected filename after >>\n");
                    delete currentSimpleCommand;
                    return false;
                }
            } else {
                fprintf(stderr, "Error: Missing filename for >>\n");
                delete currentSimpleCommand;
                return false;
            }
        }
        else if (token.type == TOKEN_INPUT) { // <
            if (i + 1 < tokens.size()) {
                TokenType next_type = tokens[i + 1].type;
                if (next_type == TOKEN_COMMAND || next_type == TOKEN_ARGUMENT) {
                    Command::_currentCommand._inputFile = strdup(tokens[i + 1].value.data());
                    i++;
                } else {
                    fprintf(stderr, "Error: Expected filename after <\n");
                    delete currentSimpleCommand;
                    return false;
                }
            } else {
                fprintf(stderr, "Error: Missing filename for <\n");
                delete currentSimpleCommand;
                return false;
            }
        }
        else if (token.type == TOKEN_ERROR) { // 2>
            if (i + 1 < tokens.size()) {
                TokenType next_type = tokens[i + 1].type;
                if (next_type == TOKEN_COMMAND || next_type == TOKEN_ARGUMENT) {
                    Command::_currentCommand._errFile = strdup(tokens[i + 1].value.data());
                    i++;
                } else {
                    fprintf(stderr, "Error: Expected filename after 2>\n");
                    delete currentSimpleCommand;
                    return false;
                }
            } else {
                fprintf(stderr, "Error: Missing filename for 2>\n");
                delete currentSimpleCommand;
                return false;
            }
        }
        else if (token.type == TOKEN_REDIRECT_AND_ERROR) { // >>&
            if (i + 1 < tokens.size()) {
                TokenType next_type = tokens[i + 1].type;
                if (next_type == TOKEN_COMMAND || next_type == TOKEN_ARGUMENT) {
                    Command::_currentCommand._outFile = strdup(tokens[i + 1].value.data());
                    Command::_currentCommand._errFile = strdup(tokens[i + 1].value.data());
                    Command::_currentCommand._append = true;
                    i++;
                } else {
                    fprintf(stderr, "Error: Expected filename after >>&\n");
                    delete currentSimpleCommand;
                    return false;
                }
            } else {
                fprintf(stderr, "Error: Missing filename for >>&\n");
                delete currentSimpleCommand;
                return false;
            }
        }
        else if (token.type == TOKEN_BACKGROUND) { // &
            Command::_currentCommand._background = true;
        }
        else if (token.type == TOKEN_EOF) {
            break;
        }
        else {
            fprintf(stderr, "Error: Unknown token.\n");
            delete currentSimpleCommand;
            return false;
        }

        lastTokenType = token.type;
    }

    // Error Checking: Pipe at end "ls |"
    if (lastTokenType == TOKEN_PIPE) {
        fprintf(stderr, "Error: Command ends with a pipe.\n");
        delete currentSimpleCommand;
        return false;
    }

    // Error Checking: Missing command "> "
    if (!hasCommand && tokens.size() > 1 && tokens[0].type != TOKEN_EOF) {
         fprintf(stderr, "Error: Missing command.\n");
        delete currentSimpleCommand;
        return false;
    }

    // Add the final SimpleCommand
    Command::_currentCommand.insertSimpleCommand(currentSimpleCommand);
    return true;
}

SimpleCommand::SimpleCommand()
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));

    _arguments[0] = NULL;
}

void SimpleCommand::insertArgument(char *argument)
{
    if (_numberOfAvailableArguments == _numberOfArguments + 1)
    {
        _numberOfAvailableArguments *= 2;
        _arguments = (char **)realloc(_arguments,
                                      _numberOfAvailableArguments * sizeof(char *));
    }

    _arguments[_numberOfArguments] = argument;

    // NULL-terminate the argument list for execvp
    _arguments[_numberOfArguments + 1] = NULL;

    _numberOfArguments++;
}

Command::Command()
{
    _numberOfAvailableSimpleCommands = 1;

    _simpleCommands = (SimpleCommand **)
        malloc(_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
    _append = 0;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
    if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands)
    {
        _numberOfAvailableSimpleCommands *= 2;
        _simpleCommands = (SimpleCommand **)realloc(_simpleCommands,
                                                    _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
    }

    _simpleCommands[_numberOfSimpleCommands] = simpleCommand;
    _numberOfSimpleCommands++;
}

void Command::clear()
{
    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            // We free arguments because parse() used strdup()
            free(_simpleCommands[i]->_arguments[j]);
        }

        free(_simpleCommands[i]->_arguments);
        free(_simpleCommands[i]);
    }

    // We free filenames because parse() used strdup()
    if (_outFile)
    {
        free(_outFile);
    }

    if (_inputFile)
    {
        free(_inputFile);
    }

    if (_errFile)
    {
        free(_errFile);
    }

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
    _append = 0;
}

void Command::print()
{
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        printf("  %-3d ", i);
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
        }
        printf("\n"); // Add newline for better formatting
    }

    printf("\n\n");
    printf("  Output       Input        Error        Background\n");
    printf("  ------------ ------------ ------------ ------------\n");
    printf("  %-12s %-12s %-12s %-12s\n", 
           _outFile ? _outFile : "default",
           _inputFile ? _inputFile : "default", 
           _errFile ? _errFile : "default",
           _background ? "YES" : "NO");
    if (_append) {
        printf("  (Output is in append mode)\n");
    }
    printf("\n\n");
}

void Command::execute()
{
    // Check for an empty command
    if (_numberOfSimpleCommands == 0 ||
        (_numberOfSimpleCommands == 1 && _simpleCommands[0]->_numberOfArguments == 0)) {
        clear();
        return;
    }

    // Handle built-in commands first
    // These commands run directly in the parent (shell) process.
    char *cmd = _simpleCommands[0]->_arguments[0];

    // Check for "exit"
    if (strcmp(cmd, "exit") == 0) {
        printf("Good bye!!\n");
        clear();
        exit(0); // Terminate the shell
    }

    // Check for "cd"
    if (strcmp(cmd, "cd") == 0) {
        char *dir;
        char *dir_to_print;

        if (_simpleCommands[0]->_numberOfArguments > 1) {
            dir = _simpleCommands[0]->_arguments[1]; // Get the directory argument
            dir_to_print = dir;
        } else {
            dir = getenv("HOME"); // No argument, default to HOME
            if (dir == NULL) {
                fprintf(stderr, "myshell: cd: HOME not set\n");
                clear();
                return;
            }
        }

        printf("Changing to directory '%s'\n", dir_to_print);

        // Use chdir() to change the shell's current directory
        if (chdir(dir) != 0) {
            perror("myshell: cd"); // Print error if chdir fails
        } else {
            char cwd_buffer[1024];
            if (getcwd(cwd_buffer, sizeof(cwd_buffer)) != NULL) {
                printf("You are now in %s\n", cwd_buffer);
            } else {
                perror("myshell: getcwd");
            }
        }

        clear();
        return; // "cd" is done, no need to fork.
    }

    // If it's not a built-in, it's an external command

    print(); // Print the command table

    // Variables for pipe logic
    int input_fd = 0; // 0 (stdin) is the default input for the first command
    pid_t *pids = (pid_t*)malloc(_numberOfSimpleCommands * sizeof(pid_t)); // Array to store all child PIDs
    int pipefd[2]; // [0] = read, [1] = write

    // Loop over every simple command in the pipeline
    for (int i = 0; i < _numberOfSimpleCommands; i++) {

        // Create a new pipe if this is not the last command
        if (i < _numberOfSimpleCommands - 1) {
            if (pipe(pipefd) == -1) {
                perror("myshell: pipe");
                free(pids);
                clear();
                return;
            }
        }

        // Fork a new child process for this simple command
        pid_t pid = fork();
        if (pid < 0) {
            perror("myshell: fork");
            free(pids);
            clear();
            return;
        }

        if (pid == 0) {
            // This is the child process

            // Rewire the child's input
            if (i == 0) {
                // First command: check for file input redirection
                if (_inputFile) {
                    int fd_in = open(_inputFile, O_RDONLY);
                    if (fd_in < 0) { perror("myshell: input file"); exit(1); }
                    dup2(fd_in, 0); // Redirect stdin (0) to come from the file
                    close(fd_in);
                }
            } else {
                // Not the first command: read from the previous pipe
                dup2(input_fd, 0); // Redirect stdin (0) to come from input_fd
                close(input_fd);   // This child doesn't need the original fd
            }

            // Rewire the child's output
            if (i == _numberOfSimpleCommands - 1) {
                // Last command: check for file output redirection
                if (_outFile) {
                    int flags = O_CREAT | O_WRONLY | (_append ? O_APPEND : O_TRUNC);
                    int fd_out = open(_outFile, flags, 0644);
                    if (fd_out < 0) { perror("myshell: output file"); exit(1); }
                    dup2(fd_out, 1); // Redirect stdout (1) to the file
                    close(fd_out);
                }
                if (_errFile) {
                    int flags = O_CREAT | O_WRONLY | (_append ? O_APPEND : O_TRUNC);
                    int fd_err = open(_errFile, flags, 0644);
                    if (fd_err < 0) { perror("myshell: error file"); exit(1); }
                    dup2(fd_err, 2); // Redirect stderr (2) to the file
                    close(fd_err);
                }
            } else {
                // Not the last command: write to the new pipe
                dup2(pipefd[1], 1); // Redirect stdout (1) to the pipe's write-end
                // This child doesn't need the pipe file descriptors
                close(pipefd[0]);
                close(pipefd[1]);
            }

            // Execute the command
            execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);
            perror("myshell"); // execvp only returns if an error occurred
            exit(1);

        } else {
            // This is the parent process

            // Manage file descriptors for the next loop iteration
            if (i > 0) {
                close(input_fd);
            }

            // If we're not the last command...
            if (i < _numberOfSimpleCommands - 1) {
                close(pipefd[1]); // ...close the new pipe's write-end (child is using it)
                input_fd = pipefd[0]; // ...save the new pipe's read-end for the *next* child
            }

            // Store the child's PID so we can wait for it
            pids[i] = pid;
        }
    } // End of for-loop

    // Parent cleanup logic

    // If this is not a background job, wait for all children to finish
    if (!_background) {
        for (int i = 0; i < _numberOfSimpleCommands; i++) {

            // Wait for this specific child to finish
            waitpid(pids[i], NULL, 0);

            // Log the termination of this foreground child
            // Use the log file name from the test script
            int fd = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd >= 0) {
                char buffer[100];
                snprintf(buffer, sizeof(buffer), "Child process %d terminated.\n", pids[i]);
                write(fd, buffer, strlen(buffer));
                close(fd);
            }
        }
    }

    // Free the PID array and clear the command
    free(pids);
    clear();
}

void Command::prompt()
{
    while (true) // FIX: Correct loop structure
    {
        printf("myshell>");
        fflush(stdout);
        std::string input;
        
        if (!std::getline(std::cin, input)) {
            // End of file (Ctrl+D)
            printf("\n");
            break;
        }

        std::vector<Token> tokens = tokenize(input);
        
        // 1. Parse the tokens into the global _currentCommand
        bool parse_ok = parse(tokens);

        // 2. If parsing was successful, execute the command
        if (parse_ok) {
            _currentCommand.execute();
        }
        
        // 3. The loop continues and re-prompts
    }
}

void log_child_termination(int sig) {
    (void)sig;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        int fd = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0) {
            return;
        }

        char buffer[100];
        snprintf(buffer, sizeof(buffer), "Child process %d terminated.\n", pid); // Use .log
        write(fd, buffer, strlen(buffer));
        close(fd);
    }
}

int main()
{
    signal(SIGCHLD, log_child_termination);
    Command::_currentCommand.prompt();
    return 0;
}
