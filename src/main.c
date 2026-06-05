#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  // TODO: Uncomment the code below to pass the first stage
  printf("$ ");     // display the prompt $

  // read user input
  char command_buffer[1024];
  fgets(command_buffer, sizeof(command_buffer), stdin);
  command_buffer[strcspn(command_buffer, "\n")] = 0;        // remove newline from command
  
  // printf("variable command: %s", command_buffer);      // DEBUG

  // print error msg 
  printf("%s: command not found\n", command_buffer);

  return 0;
}
