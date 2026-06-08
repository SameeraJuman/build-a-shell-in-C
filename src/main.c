#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  while(1) {
    int foundB = 0;
    int foundE = 0;
    printf("$ ");     // display the prompt $

    // read user input
    char command[1024];
    fgets(command, sizeof(command), stdin);
    command[strcspn(command, "\n")] = 0;        // remove newline from command
    
    // printf("variable command: %s", command);      // DEBUG

    
    if(strcmp(command, "exit") == 0) {                  // exit the shell (exit cmd)
      break;

    } else if(strncmp(command, "echo ", 5) == 0) {     // print the text after 'echo' (echo cmd) 
        char* after_echo = command + 5;
        printf("%s\n", after_echo);

    } else if (strncmp(command, "type ", 5) == 0) {       // type cmd
        char* builtin_cmd[] = {"echo", "exit", "type"};
        int i;
        int length = sizeof(builtin_cmd) / sizeof(builtin_cmd[0]);
        char* after_type = command + 5;
        for (i = 0; i < length; i++) {
          if (strcmp(after_type, builtin_cmd[i]) == 0) {
            printf("%s is a shell builtin\n", after_type);
            foundB = 1;
            foundE = 1;
            break;
          } 
        }
        if (!foundE) {      // searching for executables
          // get PATH env variable
          char p[1000];
          strcpy(p, getenv("PATH"));
          char* token = strtok(p, ":");
          while (token != NULL) {
            // check if file with the command name exists
            char filename[100];
            strcpy(filename, token);
            strcat(filename, "/");
            strcat(filename, after_type);
            if (access(filename, F_OK) == 0) {
              // check if file has execute permissions
              if (access(filename, X_OK) == 0) {
                foundE = 1;
                printf("%s is %s\n", after_type, filename);
                break;
              }
              // if file exists BUT lacks execute permissions, continue
            } 
            token = strtok(NULL, ":");
          }
        }

        if (!foundB && !foundE) {         // no executable is found in any dir
          printf("%s: not found\n", after_type);
        }
      
    } else {                              // launching external programs
        // searching for executables
        char launch_parse[100];
        char* args[100];
        int arg_count = 0;
        strcpy(launch_parse, command);
        char* cli_line = strtok(launch_parse, " ");
        while (cli_line != NULL) {
          args[arg_count]= cli_line;
          arg_count++;
          cli_line = strtok(NULL, " ");
        }
        args[arg_count] = NULL;

        char filename[100];
        char p[1000];
        strcpy(p, getenv("PATH"));
        char* token = strtok(p, ":");
        while (token != NULL) {
          // check if file with the command name exists
          strcpy(filename, token);
          strcat(filename, "/");
          strcat(filename, args[0]);
          if (access(filename, F_OK) == 0) {
            // check if file has execute permissions
            if (access(filename, X_OK) == 0) {
              foundE = 1;
              // printf("%s is %s\n", args[0], filename);
              break;
            }
            // if file exists BUT lacks execute permissions, continue
          } 
          token = strtok(NULL, ":");
        }
        if (foundE) {
          // 1. fork  2. execvp  3. wait for child process  4. parse input w strtok
          pid_t my_pid = fork();
          if (my_pid == 0) {        // child
            execvp(filename, args);
          } if (my_pid != 0) {      // main/parent
              waitpid(my_pid, NULL, 0);
              // printf("The child process ending----------\n");
            }
        } else {
        printf("%s: command not found\n", command);       // print error msg 
      }
    }

  }
  
  return 0;
}
