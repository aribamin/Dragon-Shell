#------------------------------------------
# Name : Arib Amin
# SID : 1707860
# CCID : arib1
#------------------------------------------

Design Choices


Built-in Commands:

cd: Changes the current working directory.

pwd: Prints the current directory.

exit: Exits the shell, ensures background processes are terminated, and shows total CPU time used.


Redirection:

I implemented input (<) and output (>) redirection so that users can easily read from or write to files during command execution.


Background Processes:

With the & symbol, the shell can run commands in the background. These processes are tracked and cleaned up using SIGCHLD when they finish.


Piping:

Piping (|) allows the output of one command to become the input for another, enabling more complex command combinations.


Signal Handling:

I set up handling for Ctrl+C (SIGINT) and Ctrl+Z (SIGTSTP). The shell doesn’t exit when Ctrl+C is pressed, and Ctrl+Z pauses the foreground process, which can later be resumed. Additionally, SIGCHLD is used to clean up finished background processes to prevent zombies.


System Calls Used


Process Management:

fork(): Spawns new processes to run commands.

execve(): Replaces the current process with the command to be executed.

wait4(): Waits for processes to finish and retrieves resource usage information.

exit(): Ensures proper process termination.

Input/Output Redirection:


open(): Opens files for reading or writing.

dup2(): Redirects input/output to/from files.

close(): Closes file descriptors after redirection.


Piping:

pipe(): Establishes communication between two commands, enabling piping.


Signal Handling:

signal(): Registers handlers for Ctrl+C, Ctrl+Z, and background process cleanup (SIGCHLD).

kill(): Sends signals like pause or terminate to specific processes.


Miscellaneous:

getcwd(): Retrieves the current directory.

chdir(): Changes the current directory.


Testing


Built-in Commands:

cd: Tested switching between directories.

pwd: Verified correct directory is printed.

exit: Confirmed all background processes are terminated correctly, and total CPU time is displayed.


External Commands:

Executed Unix commands like ls, ps, and cat, with and without arguments.

Tested redirection (e.g., ls > output.txt).


Redirection:

Input redirection tested with cat < input.txt.

Output redirection tested with ls > output.txt.


Background Processes:

Ran sleep 10 & to confirm the shell accepted new commands while the background process ran.


Signal Handling:

Pressed Ctrl+C to confirm the shell continued running.

Pressed Ctrl+Z to pause the process and resumed it afterward.


Piping:

Tested simple and complex pipelines, such as ls | grep filename and ps aux | grep bash | wc -l.


Sources:

Linux man pages.

"Advanced Programming in the Unix Environment" by W. Richard Stevens.

