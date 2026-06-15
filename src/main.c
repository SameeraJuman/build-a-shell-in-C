#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <readline/readline.h>
#include <readline/history.h>

void parseCommand(char* command, char* launch_parse, char** args, int* arg_index);  // detecting quotes, backslashes, splitting on spaces. 
char* findRedirect(char** args, int* fd_num, int* append_mode);    // redirecting standard output
int findPath(char* cmd, char* filename, char* p);
char* completion_generator(const char* user_input, int state);      // tab completion
int comp(const void *a, const void *b);
char** my_completion(const char* user_input, int start, int end);    // multiple matches

char launch_parse[1024];
char* args[100];
char* builtin_cmd[] = {"echo", "exit", "type", "pwd", "cd"};

// MAIN METHOD
int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  // tab completion
  rl_attempted_completion_function = my_completion;  // when the user presses TAB, call MY function to find completions
  rl_bind_key('\t', rl_complete);  // TAB is the key that triggers it
  rl_completion_append_character = ' ';    

  while(1) {
    int foundB = 0;
    int foundE = 0;

    // display the prompt $ and read user input
    char* command = readline("$ ");
    if (command == NULL) {
      break;
    }
    add_history(command);

    if(strcmp(command, "exit") == 0) {                  // exit cmd
      break;

    } else if(strncmp(command, "echo ", 5) == 0) {     // echo cmd
        // char* after_echo = command + 5;
        int arg_index = 0;
        int fd_num = 0;
        int append_mode;
        int flags;
        parseCommand(command, launch_parse, args, &arg_index);
        char* redirect_file = findRedirect(args, &fd_num, &append_mode);
        if (append_mode) {
          flags = O_WRONLY | O_CREAT | O_APPEND;
        } else {
          flags = O_WRONLY | O_CREAT | O_TRUNC;
        }
        if (redirect_file != NULL) {
          int saved = dup(1);
          int fd = open(redirect_file, flags, 0777);  
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
        int append_mode;
        int flags;
        parseCommand(command, launch_parse, args, &arg_index);
        char* redirect_file = findRedirect(args, &fd_num, &append_mode);
        if (append_mode) {
          flags = O_WRONLY | O_CREAT | O_APPEND;
        } else {
          flags = O_WRONLY | O_CREAT | O_TRUNC;
        }
        
        char filename[1024];          // PATH
        char p[2048];
        int foundE = findPath(args[0], filename, p);

        if (foundE) {
          // 1. fork  2. execvp  3. wait for child process  4. parse input w strtok
          pid_t my_pid = fork();
          if (my_pid == 0) {        // child
            if (redirect_file != NULL) {
              int fd = open(redirect_file, flags, 0644);  
              if (fd == -1) {
                return 2;
              }
              int fd2 = dup2(fd, fd_num);    // redirect stdout or stderr
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
    free(command);
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

char* findRedirect(char** args, int* fd_num, int* append_mode) {
  char* redirect_file = NULL;         
  for (int k = 0; args[k] != NULL; k++) {
    if (strcmp(args[k], ">") == 0 || strcmp(args[k], "1>") == 0) {
      *fd_num = 1;
      *append_mode = 0;
      redirect_file = args[k+1];
      args[k] = NULL;
    } else if (strcmp(args[k], "2>") == 0) {
      *fd_num = 2;
      *append_mode = 0;
      redirect_file = args[k+1];
      args[k] = NULL;
    } else if (strcmp(args[k], "2>>") == 0) {
      *fd_num = 2;
      *append_mode = 1;
      redirect_file = args[k+1];
      args[k] = NULL;
    } else if (strcmp(args[k], ">>") == 0 || strcmp(args[k], "1>>") == 0) {
      *fd_num = 1;
      *append_mode = 1;
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

char* completion_generator(const char* user_input, int state) {
  static int list_index;
  static int len;
  char* builtin_cmd_name;              
  int match_found = 0;
  char filename[1024];          // PATH
  static char p[2048];
  static char* token;
  struct dirent *de;          // readdir(dr) returns each file as this type
  static DIR *dr;             // opendir(path) returns this type
  static DIR *curr_dr;
  static DIR *nested_dr;
  char* last_slash;
  char p2[2048];
  static char dir_path[2048];
  char* fn;
  struct stat buf;
  
  if (state == 0) {   // if new word, then start from starting
    list_index = 0;
    len = strlen(user_input);
    strcpy(p, getenv("PATH"));    // PATH
    token = strtok(p, ":");
    dr = NULL;
    curr_dr = NULL;             // FILENAME
    nested_dr = NULL;
  }

  if (strchr(rl_line_buffer, ' ') != NULL) {
    if ((last_slash = strrchr(user_input, '/')) != NULL) {  // NESTED FILE
      strcpy(p2, user_input);
      last_slash = strrchr(p2, '/');
      fn = last_slash + 1;    // after / is prefix
      *(last_slash + 1) = '\0';     // p2 has path alone

      if (nested_dr == NULL) {
        strcpy(dir_path, p2);
        nested_dr = opendir(p2);
        if (nested_dr == NULL) {         
          return NULL;
        }
      }
      while ((de = readdir(nested_dr)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        if(strncmp (de->d_name, fn, strlen(fn)) == 0) {
          char full_path[2048];
          strcpy(full_path, dir_path);
          strcat(full_path, de->d_name);
          stat(full_path, &buf);
          if (S_ISDIR(buf.st_mode)) {   // its a dir
            rl_completion_append_character = '/';
          } else {
            rl_completion_append_character = ' ';
          }
          return strdup(full_path);      // return copy of match
        } 
      }
      closedir(nested_dr);
      nested_dr = NULL;

    } else {    // FILENAME and no / , DIR NAME
      // 1. get "re" (user_input)    2. Search the current dir for files that start with "re"
        if (curr_dr == NULL) {
          curr_dr = opendir(".");
          if (curr_dr == NULL) {         
            return NULL;
          }
        }
        while ((de = readdir(curr_dr)) != NULL) {
          if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
          if(strncmp (de->d_name, user_input, len) == 0) {
            char full_path[2048];
            strcpy(full_path, "./");
            strcat(full_path, de->d_name);
            stat(full_path, &buf);
            if (S_ISDIR(buf.st_mode)) {   // its a dir
              rl_completion_append_character = '/';
            } else {
              rl_completion_append_character = ' ';
            }
            return strdup(de->d_name);      // return copy of match
          } 
        }
        closedir(curr_dr);
        curr_dr = NULL;
    } 

  } else {
      while (builtin_cmd_name = builtin_cmd[list_index]) {    // return the nxt name which partially matches from the list
        list_index++;
        if(strncmp (builtin_cmd_name, user_input, len) == 0) {
          match_found = 1;
          return strdup(builtin_cmd_name);      // return copy of match
        } 
      }
      // PATH
      while (token != NULL) {
        if (dr == NULL) {     // Only open a new dir when we don't already have one open
          dr = opendir(token);      // open dir
          if (dr == NULL) {         // the dir doesn't exist or can't be opened
            token = strtok(NULL, ":");
            continue;
          }
        }
        while ((de = readdir(dr)) != NULL) {        // reads next file from the open dir
          strcpy(filename, token);
          strcat(filename, "/");
          strcat(filename, de->d_name);       // d_name — the filename
          if(strncmp (de->d_name, user_input, len) == 0) {
            if (access(filename, F_OK) == 0) {
              if (access(filename, X_OK) == 0) {
                match_found = 1;
                return strdup(de->d_name);      // return copy of match
              }
            }
          } 
        }
        closedir(dr);
        dr = NULL;                  // open a new dir
        token = strtok(NULL, ":");
      }
  }
  fprintf(stderr, "\x07");
  return ((char*)NULL);       // if no names matched, then stop
}

int comp(const void *a, const void *b) {    // sort ascending
  return strcmp(*(char**)a, *(char**)b);
}

char** my_completion(const char* user_input, int start, int end) {
  struct stat buf;
  char** matches = rl_completion_matches(user_input, completion_generator);  // generate array
  if (matches == NULL) {
    return NULL;
  }

  int g;
  for (g = 0; matches[g] != NULL; g++);     // counting matches
  if (g == 1) {
    return matches;
  } 
  if (g > 1) {
    if (rl_last_func == rl_complete) {        // 2nd tab
      qsort(matches + 1, g - 1, sizeof(matches[0]), comp);
      printf("\n");
      for (g = 1; matches[g] != NULL; g++) {
        printf("%s", matches[g]);
        char full_path[2048];
        strcpy(full_path, "./");
        strcat(full_path, matches[g]);
        stat(full_path, &buf);
        if (S_ISDIR(buf.st_mode)) {   // its a dir
          printf("/");
        } 
        printf("  ");
      }
      fflush(stdout);
      write(STDOUT_FILENO, "\n", 1);
      rl_on_new_line();
      rl_forced_update_display();
      rl_last_func = NULL;
      return NULL;
      
    } else {                                // 1st tab
      fprintf(stderr, "\x07");
      return NULL;
    }    
  }
  return matches;
}
