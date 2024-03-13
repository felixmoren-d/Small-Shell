To compile this program first type into the console
gcc -o smallsh smallsh.c
then click enter
After that you have an executable called smallsh. To run the program 
type ./smallsh and it will run

This assignment is a small shell that has three built in commands.

The first is exit which terminates all processes and jobs and then terminates the shell.

The second is cd which changes to the directory specified in the HOME environment variable. It can also take arguments which will chnage the to the directory path specified.

The third is status which either prints out the exit status or the signal of the last foreground process ran by the shell.

All other commands will be handled using fork() exec() and waitpid().
