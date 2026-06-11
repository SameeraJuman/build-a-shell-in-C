#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

void quoteEcho(char* str);           // consecutive spaces
void parseCommand(char* command, char* launch_parse, char** args, int* arg_index);  // detecting quotes, backslashes, splitting on spaces. 
char* findRedirect(char** args);    // redirecting standard output

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
        char* after_echo = command + 5;
        quoteEcho(after_echo);

        int arg_index = 0;
        parseCommand(command, launch_parse, args, &arg_index);
        char* redirect_file = findRedirect(args);
        if (redirect_file != NULL) {
          int saved = dup(1);
          int fd = open(redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);  
          int fd2 = dup2(fd, 1);
          close(fd);
          printf("%s\n", after_echo); 
          dup2(saved, 1);
          close(saved);
        } else {
          printf("%s\n", after_echo);
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
          char p[1000];
          strcpy(p, getenv("PATH"));
          char* token = strtok(p, ":");
          while (token != NULL) {
            // check if file with the command name exists
            char filename[1024];
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
        int arg_index = 0;
        parseCommand(command, launch_parse, args, &arg_index);
        char* redirect_file = findRedirect(args);
        
        char filename[1024];
        char p[2048];
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
            if (redirect_file != NULL) {
              int fd = open(redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);  // opens and creates if doesnt exist 
              if (fd == -1) {
                return 2;
              }
              int fd2 = dup2(fd, 1);
              close(fd);
            }

            execvp(filename, args);
          } if (my_pid != 0) {      // main/parent
            int status;
            waitpid(my_pid, &status, 0);
            
            fprintf(stderr, "status=%d\n", status);
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
  bool in_s_quote = false;
  bool in_d_quote = false;
  bool last_char_not_space = false;      // pretend last char was space
  for(int i = 0; i < len; i++) {
    if (str[i] == '\\' && !in_s_quote) {    // backslash
      str[j] = str[i+1];
      j++;
      i++;
    } else if (str[i] == '\'' && !in_d_quote) {  
      if (in_s_quote) {
        in_s_quote = false;     // close
      } else {
        in_s_quote = true;     // open
      }
    } else if (str[i] == '\"' && !in_s_quote) {  // double quotes
      if (in_d_quote) {
        in_d_quote = false;     // close
      } else {
        in_d_quote = true;     // open
      }
    } else if (in_s_quote || in_d_quote) {
        str[j] = str[i];
        j++;
        last_char_not_space = true;
    } else {                        // outside quotes
        if (str[i] == ' ') {
          if (last_char_not_space) {    // copy 1st space, ignore the rest
            str[j] = str[i];
            j++;
            last_char_not_space = false;
          }
      } else {                // copy e.g. letters 
          str[j] = str[i];
          j++;
          last_char_not_space = true;
        }
    }           
  }
  str[j] = '\0';
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
          *(arg_index)++;
            args[*arg_index] = launch_parse + j;
        } else {
            launch_parse[j] = launch_parse[i];
            j++;
          }
    }           
  }
  launch_parse[j] = '\0';
  *(arg_index)++;
  args[*arg_index] = NULL; 
}   

char* findRedirect(char** args) {
  char* redirect_file = NULL;         
  for (int k = 0; args[k] != NULL; k++) {
    if (strcmp(args[k], ">") == 0 || strcmp(args[k], "1>") == 0) {
      redirect_file = args[k+1];
      args[k] = NULL;
    }
  }
  return redirect_file;
}