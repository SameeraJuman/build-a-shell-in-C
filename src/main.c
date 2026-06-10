#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void quoteEcho(char* str);           // consecutive spaces

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

    if(strcmp(command, "exit") == 0) {                  // exit cmd
      break;

    } else if(strncmp(command, "echo ", 5) == 0) {     // echo cmd
        char* after_echo = command + 5;
        quoteEcho(after_echo);
        printf("%s\n", after_echo);
        
    } else if (strncmp(command, "type ", 5) == 0) {       // type cmd
        char* builtin_cmd[] = {"echo", "exit", "type", "pwd", "cd"};
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
      
    } else if (strcmp(command, "pwd") == 0) {          // pwd cmd
        char my_pwd[1024];
        if (getcwd(my_pwd, sizeof(my_pwd)) == NULL) {
          perror("getcwd error");
          exit(1);
        } 
        printf("%s\n", my_pwd);

    } else if (strncmp(command, "cd ", 3) == 0) {         // cd cmd
        char path[100];
        char* after_cd = command + 3;
        strcpy(path, after_cd);
        if (strcmp(after_cd, "~") == 0) {           // cd ~
          char* home_path = getenv("HOME");
          chdir(home_path);
        } else if (access(path, F_OK) == -1) {        // absolute paths: dir(true) -> change
          printf("cd: %s: No such file or directory\n", path);
        } else {
            chdir(path);
        }
        
    } else {                              // launching external programs
        // searching for executables
        char launch_parse[100];
        char* args[100];
        int arg_index = 0;
        strcpy(launch_parse, command);
        char* cli_line = strtok(launch_parse, " ");
        while (cli_line != NULL) {
          args[arg_index]= cli_line;
          arg_index++;
          cli_line = strtok(NULL, " ");
        }
        args[arg_index] = NULL;     // setting the last item to null

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

void quoteEcho(char* str) {
  int len = strlen(str);
  int j = 0;
  bool in_quote = false;
  bool last_char_not_space = false;      // was last char a space or letter
  for(int i = 0; i < len; i++) {
    if (str[i] == '\'') {  
      if (in_quote) {
        in_quote = false;     // close
      } else {
        in_quote = true;     // open
      }
    } else if (in_quote) {
        str[j] = str[i];
        j++;
        last_char_not_space = true;
    } else {                        // outside quotes
        if (str[i] == ' ') {
          if (last_char_not_space) {
            str[j] = str[i];
            j++;
            last_char_not_space = false;
          }
      } else {
          str[j] = str[i];
          j++;
          last_char_not_space = true;
        }
    }           
  }
  str[j] = '\0';
}
