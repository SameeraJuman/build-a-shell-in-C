#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

void parseCommand(char* command, char* launch_parse, char** args, int* arg_index);  // detecting quotes, backslashes, splitting on spaces. 
char* findRedirect(char** args, int* fd_num);    // redirecting standard output
int findPath(char* cmd, char* filename, char* p);

char launch_parse[1024];
char* args[100];

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
        // char* after_echo = command + 5;
        int arg_index = 0;
        int fd_num = 0;
        parseCommand(command, launch_parse, args, &arg_index);
        char* redirect_file = findRedirect(args, &fd_num);
        if (redirect_file != NULL) {
          int saved = dup(1);
          int fd = open(redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);  
          int fd2 = dup2(fd, fd_num);
          close(fd);
          for (int v = 1; args[v] != NULL; v++) {
            if (strlen(args[v]) == 0) continue; 
            if (v > 1) {
              printf(" ");
            }
            printf("%s", args[v]);
          }
          printf("\n");

          dup2(saved, 1);
          close(saved);

        } else {
            for (int v = 1; args[v] != NULL; v++) {
              if (strlen(args[v]) == 0) continue; 
              if (v > 1) {
                printf(" ");
              }
              printf("%s", args[v]);
            }
            printf("\n");
        }
        
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
          char filename[1024];
          char p[2048];
          foundE = findPath(after_type, filename, p);
          if (foundE) {
            printf("%s is %s\n", after_type, filename);
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
        int arg_index = 0;
        int fd_num = 0;
        parseCommand(command, launch_parse, args, &arg_index);
        char* redirect_file = findRedirect(args, &fd_num);
        
        char filename[1024];          // PATH
        char p[2048];
        int foundE = findPath(args[0], filename, p);

        if (foundE) {
          // 1. fork  2. execvp  3. wait for child process  4. parse input w strtok
          pid_t my_pid = fork();
          if (my_pid == 0) {        // child
            if (redirect_file != NULL) {
              int fd = open(redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);  
              if (fd == -1) {
                return 2;
              }
              int fd2 = dup2(fd, fd_num);    // redirect stdout
              close(fd);
            }
            
            execvp(filename, args);

          } if (my_pid != 0) {      // main/parent
              waitpid(my_pid, NULL, 0);
            }

        } else {
            printf("%s: command not found\n", command);       // print error msg 
      }
    }
  }
  return 0;
}

void parseCommand(char* command, char* launch_parse, char** args, int* arg_index) {
  strcpy(launch_parse, command);
  args[0] = launch_parse;
  int len = strlen(launch_parse);
  int j = 0;
  bool in_s_quote = false;
  bool in_d_quote = false;
  for(int i = 0; i < len; i++) {
    if (launch_parse[i] == '\\' && !in_s_quote) {    // backslash
      launch_parse[j] = launch_parse[i+1];
      j++;
      i++;
    } else if (launch_parse[i] == '\'' && !in_d_quote) {  
      if (in_s_quote) {
        in_s_quote = false;     // close
      } else {
        in_s_quote = true;     // open
      }
    } else if (launch_parse[i] == '\"' && !in_s_quote) {  // double quotes
      if (in_d_quote) {
        in_d_quote = false;     // close
      } else {
        in_d_quote = true;     // open
      } 
    } else if (in_s_quote || in_d_quote) {
        launch_parse[j] = launch_parse[i];
        j++;
      } else {                        // outside quotes
          if (launch_parse[i] == ' ') {
            launch_parse[j] = '\0';
            j++;
          (*arg_index)++;
            args[*arg_index] = launch_parse + j;
        } else {
            launch_parse[j] = launch_parse[i];
            j++;
          }
    }           
  }
  launch_parse[j] = '\0';
  (*arg_index)++;
  args[*arg_index] = NULL; 
}   

char* findRedirect(char** args, int* fd_num) {
  char* redirect_file = NULL;         
  for (int k = 0; args[k] != NULL; k++) {
    if (strcmp(args[k], ">") == 0 || strcmp(args[k], "1>") == 0) {
      *fd_num = 1;
      redirect_file = args[k+1];
      args[k] = NULL;
    } else if (strcmp(args[k], "2>") == 0) {
      *fd_num = 2;
      redirect_file = args[k+1];
      args[k] = NULL;
    }
  }
  return redirect_file;
}

int findPath(char* cmd, char* filename, char* p) {
  strcpy(p, getenv("PATH"));
  char* token = strtok(p, ":");
  while (token != NULL) {
    // check if file with the command name exists
    strcpy(filename, token);
    strcat(filename, "/");
    strcat(filename, cmd);
    if (access(filename, F_OK) == 0) {
      // check if file has execute permissions
      if (access(filename, X_OK) == 0) {
        return 1;
      }
      // if file exists BUT lacks execute permissions, continue
    } 
    token = strtok(NULL, ":");
  }
  return 0;
}