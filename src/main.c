#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  while(1) {
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
    } else {
    printf("%s: command not found\n", command);       // print error msg 
    }

  }
  
  return 0;
}
