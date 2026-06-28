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
void my_display_matches(char** matches, int num_matches, int max_length);
void reapJobs();
int findPipe(char** args);
void execBuiltin(char** b_args);

// MARK: variables
char launch_parse[1024];
char* args[100];
char* builtin_cmd[] = {"echo", "exit", "type", "pwd", "cd", "complete", "jobs", "history"};
char* complete_cmd[1024];
char* complete_path[1024];
int compl_counter = 0;
typedef struct {
  int job_num;    // starts at 1
  pid_t pid;
  char command[1024];
  char status[16];
} Jobs;
Jobs bg_jobs[1024];
int job_counter = 0;

// MAIN METHOD
int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  // tab completion
  rl_attempted_completion_function = my_completion;  // when the user presses TAB, call MY func. to find completions
  rl_bind_key('\t', rl_complete);  // TAB is the key that triggers it
  rl_completion_append_character = ' ';    
  rl_completion_display_matches_hook = my_display_matches;  // showing mult. matches, call this func.

  while(1) {
    reapJobs();   // check for completed jobs before each prompt
    int foundB = 0;
    int foundE = 0;

    // display the prompt $ and read user input
    char* command = readline("$ ");
    if (command == NULL) {
      break;
    }
    add_history(command);

    // check for pipe before any builtin handling
    int arg_index = 0;
    parseCommand(command, launch_parse, args, &arg_index);
    int pipe_index = findPipe(args);
    if (pipe_index != -1) {
        // split args into array of commands
        char** cmds[100];
        int cmd_count = 0;
        int start = 0;

        for (int i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], "|") == 0) {
                args[i] = NULL;
                cmds[cmd_count++] = args + start;
                start = i + 1;
            }
        }
        cmds[cmd_count++] = args + start;  // last command

        // create pipes: need (cmd_count - 1) pipes
        int pipefds[100][2];
        for (int i = 0; i < cmd_count - 1; i++) {
            pipe(pipefds[i]);
        }

        pid_t pids[100];
        int length = sizeof(builtin_cmd) / sizeof(builtin_cmd[0]);

        for (int i = 0; i < cmd_count; i++) {
            pids[i] = fork();
            if (pids[i] == 0) {   // child
                // connect stdin to previous pipe (not for first command)
                if (i > 0) {
                    dup2(pipefds[i-1][0], STDIN_FILENO);
                }
                // connect stdout to next pipe (not for last command)
                if (i < cmd_count - 1) {
                    dup2(pipefds[i][1], STDOUT_FILENO);
                }
                // close all pipe fds in child
                for (int j = 0; j < cmd_count - 1; j++) {
                    close(pipefds[j][0]);
                    close(pipefds[j][1]);
                }

                // check if builtin
                int is_builtin = 0;
                for (int b = 0; b < length; b++) {
                    if (strcmp(cmds[i][0], builtin_cmd[b]) == 0) {
                        is_builtin = 1; break;
                    }
                }
                if (is_builtin) {
                    execBuiltin(cmds[i]);
                    _exit(0);
                } else {
                    char cmd_file[1024], cmd_p[2048];
                    if (findPath(cmds[i][0], cmd_file, cmd_p)) {
                        execvp(cmd_file, cmds[i]);
                    } else {
                        printf("%s: command not found\n", cmds[i][0]);
                    }
                    _exit(1);
                }
            }
        }

        // parent: close all pipe fds
        for (int i = 0; i < cmd_count - 1; i++) {
            close(pipefds[i][0]);
            close(pipefds[i][1]);
        }
        // wait for all children
        for (int i = 0; i < cmd_count; i++) {
            waitpid(pids[i], NULL, 0);
        }
        free(command);
        continue;
    }

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
        
    } else if(strncmp(command, "complete ", 9) == 0) {       // complete cmd
        char* clean_args[1024];
        int arg_index = 0;
        int b = 0;
        bool foundC = false;
        parseCommand(command, launch_parse, args, &arg_index);
        for (int l = 0; args[l] != NULL; l++) {   
          if (strlen(args[l]) == 0) {
            continue;
          } else {
            clean_args[b] = args[l];    // use this array
            b++;
          }
        }
        clean_args[b] = NULL;
        if (strcmp(clean_args[1], "-C") == 0) {     // registered completer scripts
          complete_path[compl_counter] = strdup(clean_args[2]);   // store path
          complete_cmd[compl_counter] = strdup(clean_args[3]);    // store cmd
          compl_counter++;
        } else if (strcmp(clean_args[1], "-r") == 0) {
          for (int c = 0; c < compl_counter; c++) {
            if (strcmp(clean_args[2], complete_cmd[c]) == 0) {
              free(complete_cmd[c]);
              free(complete_path[c]);
              // shift everything left to fill the gap
              for (int d = c; d < compl_counter - 1; d++) {
                complete_cmd[d] = complete_cmd[d+1];
                complete_path[d] = complete_path[d+1];
              }
              complete_cmd[compl_counter - 1] = NULL;
              complete_path[compl_counter - 1] = NULL;
              compl_counter--;
              break;
            }
          }
        } else if (strcmp(clean_args[1], "-p") == 0) {    
          for (int c = 0; complete_cmd[c] != NULL; c++) {
            if (strcmp(clean_args[2], complete_cmd[c]) == 0) {
              foundC = true;
              printf("complete -C '%s' %s\n", complete_path[c], complete_cmd[c]);
            }
          }
          if (!foundC) {
            printf("complete: %s: no completion specification\n", clean_args[2]);
          }
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
        
    } else if(strncmp(command, "history ", 8) == 0) {
      
    } else if (strcmp(command, "jobs") == 0) {   // jobs cmd
        for (int i = 0; i < job_counter; i++) {     // check which jobs exited
          int status;
          pid_t result = waitpid(bg_jobs[i].pid, &status, WNOHANG);
          if (result > 0 && WIFEXITED(status)) {
            strcpy(bg_jobs[i].status, "Done");
          }
        }
        for (int i = 0; i < job_counter; i++) {
          char marker;
          if (i == job_counter - 1) {
            marker = '+';   // most recent
          } else if (i == job_counter - 2) {
            marker = '-';   // second most recent
          } else {
            marker = ' ';   // all others
          }
          if (strcmp(bg_jobs[i].status, "Done") == 0) {
            printf("[%d]%c  %-24s%s\n", bg_jobs[i].job_num, marker, bg_jobs[i].status, bg_jobs[i].command);
          } else {
            printf("[%d]%c  %-24s%s &\n", bg_jobs[i].job_num, marker, bg_jobs[i].status, bg_jobs[i].command);
          }
        }
        int i = 0;
        while (i < job_counter) {
          if (strcmp(bg_jobs[i].status, "Done") == 0) {
              for (int d = i; d < job_counter - 1; d++) {
                  bg_jobs[d] = bg_jobs[d+1];
              }
              job_counter--;
          } else {
              i++;
          }
        }

    } else {                              // launching external programs
        // searching for executables
        int arg_index = 0;
        int fd_num = 0;
        int append_mode;
        int flags;
        bool bg = false;
        parseCommand(command, launch_parse, args, &arg_index);
        for (int i = 0; args[i] != NULL; i++) {       // taking out &
          if (strcmp(args[i], "&") == 0) {
            args[i] = NULL;
            bg = true;
          } 
        }

        // check for pipe
        int pipe_index = findPipe(args);
        if (pipe_index != -1) {
            args[pipe_index] = NULL;
            char** left_args = args;
            char** right_args = args + pipe_index + 1;

            int pipefd[2];
            pipe(pipefd);

            // left child
            pid_t left_pid = fork();
            if (left_pid == 0) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);

                int is_builtin = 0;
                int length = sizeof(builtin_cmd) / sizeof(builtin_cmd[0]);
                for (int i = 0; i < length; i++) {
                    if (strcmp(left_args[0], builtin_cmd[i]) == 0) {
                        is_builtin = 1; break;
                    }
                }
                if (is_builtin) {
                    execBuiltin(left_args);
                    _exit(0);
                } else {
                    char left_file[1024], left_p[2048];
                    if (findPath(left_args[0], left_file, left_p)) {
                        execvp(left_file, left_args);
                    } else {
                        printf("%s: command not found\n", left_args[0]);
                    }
                    _exit(1);
                }
            }

            // right child
            pid_t right_pid = fork();
            if (right_pid == 0) {
                close(pipefd[1]);
                dup2(pipefd[0], STDIN_FILENO);
                close(pipefd[0]);

                int is_builtin = 0;
                int length = sizeof(builtin_cmd) / sizeof(builtin_cmd[0]);
                for (int i = 0; i < length; i++) {
                    if (strcmp(right_args[0], builtin_cmd[i]) == 0) {
                        is_builtin = 1; break;
                    }
                }
                if (is_builtin) {
                    execBuiltin(right_args);
                    _exit(0);
                } else {
                    char right_file[1024], right_p[2048];
                    if (findPath(right_args[0], right_file, right_p)) {
                        execvp(right_file, right_args);
                    } else {
                        printf("%s: command not found\n", right_args[0]);
                    }
                    _exit(1);
                }
            }

            close(pipefd[0]);
            close(pipefd[1]);
            waitpid(left_pid, NULL, 0);
            waitpid(right_pid, NULL, 0);
            free(command);
            continue;
        }

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
              if (bg) {
                printf("[%d] %d\n", job_counter + 1, my_pid);   // print child pid
                bg_jobs[job_counter].job_num = job_counter + 1;
                bg_jobs[job_counter].pid = my_pid;
                // strip trailing " &" before storing
                char stored_cmd[1024];
                strcpy(stored_cmd, command);
                int cmd_len = strlen(stored_cmd);
                if (cmd_len >= 2 && stored_cmd[cmd_len-1] == '&' && stored_cmd[cmd_len-2] == ' ') {
                    stored_cmd[cmd_len-2] = '\0';
                }
                strcpy(bg_jobs[job_counter].command, stored_cmd);
                strcpy(bg_jobs[job_counter].status, "Running");
                job_counter++;
                continue;
            } else {
              waitpid(my_pid, NULL, 0);
            }
          }

        } else {
            printf("%s: command not found\n", command);       // print error msg 
      }
    }
    free(command);
  }
  return 0;
}

// MARK: CUSTOM FUNCT.
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
  char* last_space;
  char curr_cmd[1024];
  char* curr_path = NULL;
  static char* comp_results[1024];
  static int comp_result_count = 0;
  static int comp_result_index = 0;
  
  if (state == 0) {   // if new word, then start from starting
    for (int i = 0; i < comp_result_count; i++) {
      free(comp_results[i]);
      comp_results[i] = NULL;
    }
    comp_result_count = 0;
    comp_result_index = 0;
    list_index = 0;
    
    len = strlen(user_input);
    strcpy(p, getenv("PATH"));    // PATH
    token = strtok(p, ":");
    dr = NULL;
    curr_dr = NULL;             // FILENAME
    nested_dr = NULL;
  }

  if (strchr(rl_line_buffer, ' ') != NULL) {
    last_space = strchr(rl_line_buffer, ' ');      // is completer stored?
    strncpy(curr_cmd, rl_line_buffer, last_space - rl_line_buffer);
    curr_cmd[last_space - rl_line_buffer] = '\0';
    for (int i = 0; complete_cmd[i] != NULL; i++) {
      if (strcmp(curr_cmd, complete_cmd[i]) == 0) {
        curr_path = complete_path[i];
      }
    }
    // forking process w pipe
    if (curr_path != NULL) {
      if (state == 0 && comp_result_count == 0) {
        char pipe_buf[4096];
        int pipefd[2];    // create pipe
        pipe(pipefd);
        pid_t my_pid = fork();
        if (my_pid == 0) {        // child, writing
            close(pipefd[0]);        // no reading
            dup2(pipefd[1], 1);    // redirect file to stdout 
            close(pipefd[1]);

            // build the word immediately before user_input in rl_line_buffer
            char prev_word[1024] = "";
            char line_copy[1024];
            strcpy(line_copy, rl_line_buffer);
            char* tok = strtok(line_copy, " ");  // find the word before user_input
            char* prev = NULL;
            char* found = NULL;
            while(tok != NULL) {
              char* next = strtok(NULL, " ");
              if (next == NULL) {
                if (prev != NULL) {
                  strcpy(prev_word, prev);
                  break;
                }
              }
              prev = tok;
              tok = next;
            }

            // COMP_LINE and COMP_POINT 
            char comp_point_str[32];
            snprintf(comp_point_str, sizeof(comp_point_str), "%zu", strlen(rl_line_buffer));
            setenv("COMP_LINE", rl_line_buffer, 1);
            setenv("COMP_POINT", comp_point_str, 1);

            char* exec_args[] = {curr_path, curr_cmd, (char*)user_input, prev_word, NULL};
            execvp(curr_path, exec_args);
            _exit(1);

          } else {                        // main/parent, reading
              close(pipefd[1]);
              int total = 0;
              int bytes;
              while ((bytes = read(pipefd[0], pipe_buf + total, sizeof(pipe_buf) - 1 - total)) > 0) {
                total += bytes;
              }
              waitpid(my_pid, NULL, 0);
              close(pipefd[0]);
              pipe_buf[total] = '\0';

              char* line = strtok(pipe_buf, "\n");
              while (line != NULL && comp_result_count < 1024) {
                comp_results[comp_result_count++] = strdup(line);
                line = strtok(NULL, "\n");
              }
          }
      }
      if (comp_result_index < comp_result_count) {
        return strdup(comp_results[comp_result_index++]);
      }
      return NULL;
    }

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
    if (strlen(matches[0]) > strlen(user_input)) {
      rl_completion_suppress_append = 0;
      return matches;
    }
    if (rl_last_func == rl_complete) {        // 2nd tab
      qsort(matches + 1, g - 1, sizeof(matches[0]), comp);
      return matches;  
    } else {                             // 1st tab
      // check if matches[0] (the common prefix) is longer than user_input
      if (strlen(matches[0]) > strlen(user_input)) {
          return matches;
      } else {
          fprintf(stderr, "\x07");
          for (int h = 0; matches[h] != NULL; h++) free(matches[h]);
          free(matches);
          return NULL;
          }
        }
  }
  return matches;
}

void my_display_matches(char** matches, int num_matches, int max_length) {
  struct stat buf;
  write(STDOUT_FILENO, "\n", 1);
  for (int h = 1; matches[h] != NULL; h++) {
      write(STDOUT_FILENO, matches[h], strlen(matches[h]));
      char full_path[2048];
      snprintf(full_path, sizeof(full_path), "./%s", matches[h]);
      stat(full_path, &buf);
      if (S_ISDIR(buf.st_mode)) write(STDOUT_FILENO, "/", 1);
      write(STDOUT_FILENO, "  ", 2);
  }
  write(STDOUT_FILENO, "\n", 1);
  rl_on_new_line();
  rl_redisplay();
}

void reapJobs() {
  for (int i = 0; i < job_counter; i++) {   // check for exited jobs
      int status;
      pid_t result = waitpid(bg_jobs[i].pid, &status, WNOHANG);
      if (result > 0 && WIFEXITED(status)) {
          strcpy(bg_jobs[i].status, "Done");
      }
  }

  int i = 0;      // print and remove Done jobs
  while (i < job_counter) {
      if (strcmp(bg_jobs[i].status, "Done") == 0) {
          char marker;
          if (i == job_counter - 1) marker = '+';
          else if (i == job_counter - 2) marker = '-';
          else marker = ' ';
          printf("[%d]%c  %-24s%s\n", bg_jobs[i].job_num, marker, bg_jobs[i].status, bg_jobs[i].command);
          // shift left
          for (int d = i; d < job_counter - 1; d++) {
              bg_jobs[d] = bg_jobs[d+1];
          }
          job_counter--;
      } else {
          i++;
      }
  }
}

int findPipe(char** args) {
  for (int i = 0; args[i] != NULL; i++) {
      if (strcmp(args[i], "|") == 0) {
          return i;   // return index of the pipe
      }
  }
  return -1;
}

void execBuiltin(char** b_args) {
  if (strcmp(b_args[0], "echo") == 0) {
      for (int v = 1; b_args[v] != NULL; v++) {
          if (strlen(b_args[v]) == 0) continue;
          if (v > 1) printf(" ");
          printf("%s", b_args[v]);
      }
      printf("\n");
  } else if (strcmp(b_args[0], "type") == 0) {
      char* after_type = b_args[1];
      int length = sizeof(builtin_cmd) / sizeof(builtin_cmd[0]);
      int foundB = 0, foundE = 0;
      for (int i = 0; i < length; i++) {
          if (strcmp(after_type, builtin_cmd[i]) == 0) {
              printf("%s is a shell builtin\n", after_type);
              foundB = 1; foundE = 1;
              break;
          }
      }
      if (!foundE) {
          char filename[1024], p[2048];
          foundE = findPath(after_type, filename, p);
          if (foundE) printf("%s is %s\n", after_type, filename);
      }
      if (!foundB && !foundE) printf("%s: not found\n", after_type);
  } else if (strcmp(b_args[0], "pwd") == 0) {
      char my_pwd[1024];
      getcwd(my_pwd, sizeof(my_pwd));
      printf("%s\n", my_pwd);
  }
}