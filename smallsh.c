/*
Author: Felix Moren
Project: Small Shell
File Name: smallsh.c
Class: CS344
Date: 5/22/2023
Description: Small terminal shell that can be used to execute several 
different commands
*/

#include <sys/types.h>
#include <unistd.h> 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <libgen.h>

int foregroundonly = 0;
//struct used to hold elements from the terminal input
struct splitString{
    char* pathname; //holds the file path when using CD
    char** argv; //an array of arguments from the input
    char* ifile; //input file for file redirection
    char* ofile; //output file for file redirection
    int amp; //boolean to signal if a process is in the background or not
};
//ReplaceDollarSign
//Function that replaces all instances of $$ in a string with the pid of the
//parent function
void replaceDollarSign(char* str) {
    //initializing variables
    char* dollarPos; 
    char pidStr[10];
    char tempStr[2048];
    int pid;
    //getting the pid from the main function and turning it into a string
    pid = getpid();
    sprintf(pidStr, "%d", pid);
    //finds all instances of $$
    dollarPos = strstr(str, "$$");
    while (dollarPos != NULL) {
        strncpy(tempStr, str, dollarPos - str);    // Copy characters before $$
        tempStr[dollarPos - str] = '\0';
        strcat(tempStr, pidStr);                   // Append the process ID string
        strcat(tempStr, dollarPos + 2);            // Append characters after $$

        strcpy(str, tempStr);                      // Copy the modified string back to original

        dollarPos = strstr(str, "$$");             // Find the next occurrence of $$
    }
}
//SplitString
//Function that splits the string into the different parts of the 
//split string struct and returns that struct
struct splitString* readstring(char* string){
    replaceDollarSign(string); //replaces all the $$ in the string
    //initializing variables
    int argcount;
    struct splitString* SS = malloc(sizeof(struct splitString));
    char* saveptr1;
    int tokenCount = 0;
    SS->amp = 0;

    char* token = strtok_r(string, " ", &saveptr1); //uses strtok_r to get the first value in the string
   
    char* tokens[512] = { NULL }; //initializes an array of strings with the set value of NULL

    //runs until strtok_r returns null
    //saves all tokens in the string in the tokens array
    while (token != NULL && tokenCount < 511) {
        tokens[tokenCount] = strdup(token);
        token = strtok_r(NULL, " ", &saveptr1);
        tokenCount++;

    }
    //removes the endline from all of the tokens (strtok_r adds an endline)
    int i;
    for(i = 0; i < tokenCount; i++){
        tokens[i][strcspn(tokens[i], "\n")] = 0;
    }
    
    for (i = 0; i < tokenCount; i++) {
        if (strcmp(tokens[i], "<") == 0) { //if the token is <, set the next token as the ifile variable
            if(tokens[i + 1] != NULL){
                SS->ifile = tokens[i + 1];
                i++;
            }
        } else if (strcmp(tokens[i], ">") == 0) { //if token is >, set the next token as the ofile variable
            if(tokens[i + 1] != NULL){
                SS->ofile = tokens[i + 1];
                i++;
            }
        } else if (strcmp(tokens[i], "&") == 0) { //if there is an ampersand, set the amp value to 1
            SS->amp = 1;
        } else{ //else add up argcount
            argcount++;
        }
    }
    //sets pathname
    if(argcount > 0){
        SS->pathname = tokens[1];
    }
    //initializes the argv array and puts all remaining tokens into the argv array
    SS->argv = malloc((argcount + 1) * sizeof(char*));
    for(i = 0; i < sizeof(SS->argv) / sizeof(char*); i++){
        SS->argv[i] = NULL;
    }
    for(i = 0; i < argcount; i++){
        SS->argv[i] = strdup(tokens[i]);
    }

    return SS;

}
//change_directory
//Function that changes the directory to a specified path
//if no value is entered goes to the home directory
void change_directory(char* pathname, char* homedir){
    char CWD[256];

    if(pathname == NULL){ //if no pathname goes to home directory
        chdir(homedir);
    }else if(chdir(pathname) == -1){//runs pathname if there is an error, adds home directory to the pathname to see if 
        getcwd(CWD, sizeof(CWD)); //that works
        strcat(CWD, pathname);
        if(chdir(CWD) == -1){ //if none work prints error
            printf("That directory doesn't exist");
        }

    }
}
//run_cmnd
//Function that runs a command through a child process in the foreground
//Returns the pid
int run_cmnd(struct splitString* cmnd, int status){
    //saves the current I/O incase of file redirection
    int savedOUT = dup(1); 
    int savedIN = dup(2);
    //initializing variables
    int result;
    int sourceFD;
    if(cmnd->ifile != NULL){ //if theres an infile
        sourceFD = open(cmnd->ifile, O_RDONLY); //opens input file as read only
        if(sourceFD == -1){
            perror("source open()");
        }
        result = dup2(sourceFD, 0); //redirects stdin to input file
    }
    int targetFD;
    if(cmnd->ofile != NULL){ //if theres an outfile
        targetFD = open(cmnd->ofile, O_WRONLY | O_CREAT | O_TRUNC, 0644); //opens or creates an output file
        if(targetFD == -1){
            perror("target open()");            
        }
        result = dup2(targetFD, 1); //redirecets stdout to the output file
    }

    // Fork a new process
	pid_t spawnPid = fork();
    int i;
    
    

	switch(spawnPid){
	case -1:
		perror("fork()\n");
		break;
	case 0:

        signal(SIGINT, SIG_DFL); //treats sigint as it normally would (shell ignores it)
		// In the child process
		execvp(cmnd->argv[0], cmnd->argv);
		// exec only returns if there is an error
		perror("execvp");
        exit(2);
		break;
	default:
		// In the parent process
		// Wait for child's termination
		spawnPid = waitpid(spawnPid, &status, 0);
		break;
	}

    //redirects input and output to their original spots
    dup2(savedIN, 0);
    dup2(savedOUT, 1);

    return status;
}
//runbackgroundcmnd
//Function that runs a command in the background (parent function doesn't wait for it)
//returns the pid of the child function
int runbackgroundcmnd(struct splitString* cmnd, int status){
    //saves the I/O incase of file redirection
    int savedOUT = dup(1);
    int savedIN = dup(2);
    //initializing variables
    int result;
    int sourceFD;

    if(cmnd->ifile != NULL){//if there is an infile
        sourceFD = open(cmnd->ifile, O_RDONLY); //opens file as readonly
        if(sourceFD == -1){
            perror("source open()");
            
        }
        result = dup2(sourceFD, 0); //redirects input to ifile
    }else{
        int sourceFD = open("/dev/null", O_RDONLY); //if there is no input file changes input to /dev/null to not interfere with the terminal
        result = dup2(sourceFD, 0);
    }
    int targetFD;
    if(cmnd->ofile != NULL){ //if there is an outfile
        targetFD = open(cmnd->ofile, O_WRONLY | O_CREAT | O_TRUNC, 0644); //opens or creates a file to output to
        if(targetFD == -1){
            perror("target open()");
            
        }
        result = dup2(targetFD, 1); //redirects ouput to ofile
    }else{
        int targetFD = open("/dev/null", O_WRONLY); //if there is no output file changes the output to /dev/null to not mess with the terminal
        result = dup2(targetFD, 0);
    }

    // Fork a new process
	pid_t spawnPid = fork();
    int i;
    switch(spawnPid){
        case -1:
            perror("fork()\n");
            break;
        case 0:
            execvp(cmnd->argv[0], cmnd->argv);
            perror("execvp");
            exit(2);
		    break;     
        default: //no code since the parent function is continuing before the child function exits
            break;     

    }
    //resets the input and output
    dup2(savedIN, 0);
    dup2(savedOUT, 1);
    //prints the background input
    printf("Background pid is %d\n", spawnPid);
    return spawnPid;
}

//statusCmnd
//checks the exit status of the most recently executed foreground process
void statusCmnd(int status){
    if(status == 0){ //if the status is 0 print the exit status as 0
        printf("Exit value %d\n", status);
        //else it prints either that the process exited normally or because of a signal
    }else if(WIFEXITED(status)){ 
        printf("Exit value: %d\n", WEXITSTATUS(status));
    }else if(WIFSIGNALED(status)){
        printf("Terminated by signal %d\n", WTERMSIG(status));
    }
}
//pid_checker
//Function that goes through an array of pid's and checks the status of them
//Prints if any have been terminated and removes them from the array
void pid_checker(pid_t* pidarray){
    //initializing variables
    int i;
    int childstatus;
    
    for(i = 0; i < sizeof(pidarray) / sizeof(int); i++){
        if(pidarray[i] != 0){//all pid's are set to 0 at the start, only checks values that have been changed to a pid
            int result = waitpid(pidarray[i], &childstatus, WNOHANG);
            if(result != 0){//if the value of return isn't 0 then the process has exited
                if(WIFEXITED(childstatus)){ //if true the process terminated normally
                    printf("%d was terminated normally\n", pidarray[i]);
                }else if(WEXITSTATUS(childstatus)){ //if true the process exited with a different status
                    printf("%d has an exit status of %d\n", pidarray[i], childstatus);
                }else if(WIFSIGNALED(childstatus)){ //if true the process was ended by a signal
                    int signal = WTERMSIG(childstatus);
                    printf("%d was terminated by signal %d\n", pidarray[i], signal);
                }
                //sets the value to 0 after its been checked
                pidarray[i] = 0;
            }
        }
    }
}

//handle_SIGTSTP
//Function that runs whenever the sigtstp signal is caught.
//Used to enter foreground mode
void handle_SIGTSTP(int signo){
    //either exits or enters foreground mode which is declared by a global "froegroundonly"
    if(foregroundonly == 0){char* message = "\nEntering foreground-only mode\n:"; write(STDOUT_FILENO, message, 34);}
    else{char* message = "\nExiting foreground-only mode\n:"; write(STDOUT_FILENO, message, 32);}
    foregroundonly = !foregroundonly;
}
//main
//Main function where the user is prompted and the instructions for other functions happen
int main(){
    //initializes the homedirectory
    char homedir[256];
    getcwd(homedir, sizeof(homedir)); 
    //initializing different variables
    pid_t pidarray[256];
    int pidarraycount = 0;
    int exitval = 0;

    signal(SIGINT, SIG_IGN);
    // Initialize SIGINT_action struct to be empty
    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};

    // Fill out the SIGINT_action struct
    // Register handle_SIGINT as the signal handler
    SIGINT_action.sa_handler = SIG_IGN;
    SIGTSTP_action.sa_handler = handle_SIGTSTP;

    // Block all catchable signals while handle_SIGINT is running
    sigfillset(&SIGINT_action.sa_mask);

    sigfillset(&SIGTSTP_action.sa_mask);
    // No flags set
    SIGINT_action.sa_flags = 0;
    SIGTSTP_action.sa_flags = SA_RESTART;

    // Install our signal handler
    sigaction(SIGINT, &SIGINT_action, NULL);
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    int status = 0;
    //gets the parent pid
    pid_t pid = getpid();

    char cwd[256];
    
    while(exitval == 0){
        //runs pid_checker at the start of every display
        pid_checker(pidarray);
        //initializing variables for getline
        char *string = NULL;
        size_t size = 0;
        size_t chars_read;

        printf(":");
        fflush(stdout);
        chars_read = getline(&string, &size, stdin);

        string[strcspn(string, "\n")] = 0; //removes endline from getline
        if(strcmp(string, "") != 0 && string[0] != '#'){//if input is nothing or a comment ignores it
            //splits the string into a splitstring struct
            struct splitString* newString = readstring(string);
            //runs 3 different commands based on the first input
            //if none are found tries to run command in runcmnd through event
            if(strcmp(newString->argv[0], "exit") == 0){
                exitval = 1;
            }
            else if(strcmp(newString->argv[0], "cd") == 0){
                change_directory(newString->pathname, homedir);
            }
            else if(strcmp(newString->argv[0], "status") == 0){
                statusCmnd(status);
            }else{
                if(newString->amp == 1 && foregroundonly == 0){
                    pidarray[pidarraycount] = runbackgroundcmnd(newString, status);
                    pidarraycount++;
                }else{
                    status = run_cmnd(newString, status);
                }
                
            }
        }
        
    } 
}

