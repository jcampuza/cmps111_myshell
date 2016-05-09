#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

// get the external function get_args
extern char ** get_args();

// debug flag to print arguments out
int debug_f = 0;


// if debug is on print the arguments that were supplied
void debug_print_args(char **args) {
    if (debug_f == 1) {
        for (int i = 0; args[i] != NULL; i++) {
            printf ("Argument %d: %s\n", i, args[i]);
        }
        if (args[0] == NULL) {
            printf ("No arguments on line!\n");
        }
    }
}

/* the change_directory function will grab an args array, as 
   well the starting directory, which should be initialized as
   the shell begins. If a path is given as an arg, change directory,
   otherwise change to the starting directory
*/
void change_directory(char **args, char *start_directory) {
    if (args[1] == NULL) {
        if(chdir(start_directory) < 0) {
            perror("chdir");
        } 
    } else {
        if (args[2]) {
            printf("cd: Too many arguments\n");    
        } else if (chdir(args[1]) < 0) {
            printf("%s: No such file or directory.\n", args[1]);
        }
    } 
}



/* execute a set of null terminated arguments
   this function will check to see if there are special characters
   >, <, >>, or | and perform the unix equivalent for these special
   characters, i.e. file redirection/piping. If an & follows any of
   these characters, then stderr is passed along with stdout to the 
   same file/pipe.
*/
int execute(char **args, char *start_directory) {
    pid_t pid;
    pid_t w_pid;
    int status; 
    char *input_file, *output_file;    
    int tmp_in, tmp_out;
    int fd_in, fd_out;
    int in_flag = 0, out_flag = 0, err_flag = 0, pipe_flag = 0; 
    char **next_cmd, num_commands = 1;
    int pipefd[2];
    char *in_file, *out_file;    


    /* if the first argument is exit, and no other arguments are 
       provided then exit*/
    if (!strcmp (args[0], "exit") && args[1] == NULL) {
                printf("Exiting...\n");
                exit(EXIT_SUCCESS);
            } else if (strcmp(args[0], "cd") == 0) {
                change_directory(args, start_directory);  
                return 0;
            }

    /* save the next_cmd to point to the same location as args in case
       a pipe is present. */
    next_cmd = args;


    /* The following loop will check for special characters. When a
       special character is found, we set that value to null in the
       array simply because when execvp is called, the second argument
       is a null terminated array or pointer to char *s. So if we catch
       every special character, we can successfully avoid handing that
       as an argument to a command. Only one input file or output file
       is supported and so we keep track of if one is already found,
       and return if a user enters another redirect of the same nature.
    */
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            if (in_flag > 0) {
                printf("Ambiguous output redirect.\n");
            }
            in_flag = 1;
            args[i] = NULL;
            in_file = args[i+1];
        } else if (strcmp(args[i], ">") == 0) {
            if (out_flag > 0) {
                printf("Ambiguous output redirect.\n");
                return 1;
            }
            out_flag = 1;
            args[i] = NULL;
            out_file = args[i+1];
        } else if (strcmp(args[i], ">&") == 0) {
            if (out_flag > 0) {
                printf("Ambiguous output redirect.\n");
                return 1;
            }
            err_flag = 1;
            out_flag = 1;
            args[i] = NULL;
            out_file = args[i+1];
        } else if (strcmp(args[i], ">>") == 0) {
            if (out_flag > 0) {
                printf("Ambiguous output redirect.\n");
                return 1;
            }
            out_flag = 2;
            args[i] = NULL;
            out_file = args[i+1];
        } else if (strcmp(args[i], ">>&") == 0) {
            if (out_flag > 0) {
                printf("Ambiguous output redirect\n");
                return 1;
            }
            out_flag = 2;
            err_flag = 1;
            args[i] = NULL;
            out_file = args[i+1];
        } else if (strcmp(args[i], "|") == 0) {
            /* if we find an output redirect on the LHS of a pipe tell
               the user that this is not intended */
            if (out_flag > 0) {
                printf("Ambiguous output redirect\n");
                return 1;
            }
            /* set the address of the second command array so that
               we can execute this command using execvp later */
            next_cmd = &args[i + 1];
            num_commands = 2;
            args[i] = NULL;
        }       
    }
  
    /* make a copy of stdin and stdout */
    tmp_in = dup(0);
    tmp_out = dup(1);

    /* if an input redirect is present try to open the file given
       as the next arg, or default back to stdin */
    if (in_flag > 0) {
        if((fd_in = open(in_file, O_RDONLY)) < 0) {
            perror("open");
            return 1;
        }
    } else {
        fd_in=dup(tmp_in);
    }
    
    /* while there are still commands to be run (currently can only 
       pipe between two) we will loop */
    while (num_commands) {
        if (num_commands == 1) { 
            /* again setup for piping, dup fd_in for if an input,
               redirect was specified set the output end of the 
               program if a file output redirect is present */
            dup2(fd_in, 0);
            close(fd_in);
            
            /* out_flag, 1 = create, 2 = append, otherwise just use
               stdout */
            if (out_flag == 1) {
                fd_out = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            } else if (out_flag == 2) {
                fd_out = open(out_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            } else {
                fd_out = dup(tmp_out);
            }
        
            if (err_flag) {
                dup2(fd_out, 2);
                close(fd_out);
            } 
            dup2(fd_out, 1);
            close(fd_out);

            /* fork the child, execute the last set of args (only set
               if only one command) and decrement commands left (should
               be 0 at this point) */
            pid = fork(); 
            if (pid == 0) {
            //this is the child process, so execute command here
            execvp(next_cmd[0], next_cmd);
            perror("execvp");
            exit(EXIT_FAILURE);
            } else if (pid < 0) {
                perror("fork");
            }
            num_commands--;
        } else {

            /* setup for piping, dup fd_in with stdin, pipe
               then set the output and input ends of the pipe
               based on whether file redirection was specified
               if the stderr also needs to be redirected, do that
               as well to the output of the pipe. */
            dup2(fd_in, 0);
            close(fd_in);
            pipe(pipefd);
            fd_out = pipefd[1];
            fd_in = pipefd[0];

            if (err_flag) {
                dup2(fd_out, 2);
                close(fd_out);
            }
            dup2(fd_out, 1);
            close(fd_out);

            /* fork the child, execute the first set of args
               and decrement the number of commands left */
            pid = fork();
            if (pid == 0) {
                execvp(args[0], args);
                perror("execvp");
                exit(EXIT_FAILURE);
            } else if (pid < 0) {
                perror("fork");
            }
            num_commands--;
        }
    }

    /* restoring initial file descriptors*/
    dup2(tmp_in, 0);
    dup2(tmp_out, 1);
    close(tmp_in);
    close(tmp_out);
    /* parent waits here for children to finish */
    if (pid > 0) {
        wait(&status);
    }
    return 0;
}

/* here is the entrance to the program */
int main() {

    /* store the initial directory that the shell was started in */
    char start_directory[2048];
    if (getcwd(start_directory, sizeof(start_directory)) == NULL) {
        perror("getcwd");
    }
    char **     args;
    char **     next_cmd;

    printf ("Welcome to myshell. Type 'exit' to quit\n");

    /* here is the loop for the shell, print the prompt. Get the args
       that the user enters. If nothing is entered, just continue. */
    while (1) {
        printf ("=> ");
        args = get_args();
        debug_print_args(args);
        if (args[0] == '\0') {
            continue;
        }


        /* save a pointer for the next command to be the start of
           the args. We will loop and if we find a semicolon, replace
           it with null and execute the command. Because of how execvp
           is used, and due to NULL checking in all the code, we will 
           always terminate at that NULL. Finally set the starting
           pointer for the next command to the argument following the
           semicolon */
        next_cmd = args;
        for (int i = 0; args[i] != NULL; i++) {
            if(strcmp(args[i], ";") == 0) {
                
                args[i] = NULL;
                execute(next_cmd, start_directory);
                next_cmd = &args[i + 1];
            }
        }
        /* execute the last command after checking for semicolons
           or the first one if there were no semicolons */ 
        execute(next_cmd, start_directory);       
    }
}
