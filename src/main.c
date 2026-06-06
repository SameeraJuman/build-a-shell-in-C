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

    // exit the shell (exit cmd)
    if(strcmp(command, "exit") == 0) {
      break;
    }

    // print error msg 
    printf("%s: command not found\n", command);

  }
  
  return 0;
}
