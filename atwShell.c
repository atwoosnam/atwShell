/*
    atwShell.c

    Andrew T. Woosnam 
    Sept 23, 2017

    Thanks to help from
    Sherri Goings & Jeff Ondich

    TO DO:
    *   pipe with redirects doesn't fully work

 */

#include    <stdlib.h>
#include    <stdio.h>
#include    <unistd.h>
#include    <string.h>
#include    <sys/types.h>
#include    <sys/wait.h>
#include    <fcntl.h>
#include    <ctype.h>

#define clear() printf("\033[H\033[J")

char** readLineOfWords();
int isValid(char c);


// A line may be at most 100 characters long, which means longest word is 100 chars,
// and max possible tokens is 51 as must be space between each
size_t MAX_WORD_LENGTH = 100;
size_t MAX_NUM_WORDS = 51;

int main() {
  int wait = 1; // default = wait for child processes to finish
  char* outfile = NULL;
  char* infile = NULL;
  int numPipes = 0;
  char* leftOfPipe[MAX_NUM_WORDS];
  char* rightOfPipe[MAX_NUM_WORDS];
  int pipeArr[2]; // this pipe array may or may not be used
  int i; int j;

  clear();

  printf("Enter a shell command: ");
  fflush(stdout);

  while (1) {
    wait = 1;
    // clean up before next command
    outfile = NULL;
    infile = NULL;
    for (i = 0; i < MAX_NUM_WORDS; i++) {
      leftOfPipe[i] = NULL;
      rightOfPipe[i] = NULL;
    }
    numPipes = 0;
    i = 0; j = 0;

    char** words = readLineOfWords();

    if (words[i] == NULL) {
      printf("Enter a shell command: "); fflush(stdout);
      continue;
    } else if (strcmp(words[0], "-1") == 0) {
      // value of -1 is  signal that readLineOfWords() read > 100 characters
      printf("ERROR: Commands can only contain a maximum of 100 characters.\n");
      printf("\nEnter a shell command: "); fflush(stdout);
      continue;
    } else {
      int invalidChar = 0;
      while (words[i] != NULL) {
        // check for invalid characters
        while (words[i][j] != '\0') {
          if (!(isValid(words[i][j]))) {
            printf("ERROR: invalid character: %c\n", words[i][j]);
            invalidChar = 1;
            break;
          }
          j++; // next char
        }

        // scan for special operators, record infile/outfile appropriately
        char token = (* words[i]);
        j = 0;

        if (token == '|') {
          // signal that we have a piped command
          numPipes++;
        }
        else if (token == '>') {
          // stop execution before it tries to execute anything after '>'
          words[i] = NULL;
          outfile = words[i + 1];
        }
        else if (token == '<') {
          // stop execution before it tries to execute anything after '>'
          words[i] = NULL;
          infile = words[i + 1];
        }
        else if (token == '&') {
          words[i] = NULL;
          wait = 0; // don't wait for child to finish
        }
        i++; // next word in command string
      }

      if (invalidChar == 1) {
        invalidChar = 0;
        printf("\nEnter a shell command: "); fflush(stdout);
        continue; // new command prompt
      }

      printf("\n");

      // special pre-fork setup in case of piping
      if (numPipes != 0) {
        pipe(pipeArr);
        i = 0;

        // store "pre-pipe" command
        while ((* words[i]) != '|') {
          leftOfPipe[i] = words[i];
          i++;
        }
        leftOfPipe[i] = NULL;
        i++;
        j = 0;

        // store "post-pipe" command
        while (words[i] != NULL) {
          rightOfPipe[j] = words[i];
          i++; j++;
        }
        rightOfPipe[i] = NULL;
      }

      int pid = fork(); // every command should fork at least 1 child

      if ( pid == 0 ) { // child process

        if (outfile != NULL) {
          int newfd = open(outfile, O_CREAT | O_WRONLY, 0644);
          dup2(newfd, 1);
        }
        if (infile != NULL) {
          int newfd = open(infile, O_CREAT | O_RDONLY, 0644);
          dup2(newfd, 0);
        }
        // piping case
        if (numPipes != 0) {
          // execute pre-pipe command
          close(pipeArr[0]);
          dup2(pipeArr[1], 1);
          int err = execvp(leftOfPipe[0], leftOfPipe);
          printf("EXECUTION ERROR: %d\n", err);
          break;
        } else { // not piping
          int err = execvp(words[0], words);
          printf( "EXECUTION ERROR: %d\n", err );
          break;
        }
      }

      else { // parent
        if (wait == 1) {
          waitpid(pid, NULL, 0);
        }
        if (numPipes != 0) {
          // if we're piping, we'll need another child
          int pidLast = fork();
          if (pidLast == 0) {
            // execute post-pipe command
            close(pipeArr[1]);
            dup2(pipeArr[0], 0);
            int err = execvp(rightOfPipe[0], rightOfPipe);
            printf( "EXECUTION ERROR: %d\n", err );
            break;
          }

          else { // parent
            close(pipeArr[0]);
            close(pipeArr[1]);

            if (wait == 1) {
              waitpid(pidLast, NULL, 0);
            }
          }
        }
      }
      printf("\nEnter a shell command: "); fflush(stdout);
    }
  }
  return 0;
}


/*
 * Function to check whether or not c is an acceptable character
 * to appear in an input command to the shell.
 * Returns 1 if c is valid, returns 0 if c is invalid.
 */
int isValid(char c) {
  int result = 0;
  char otherValids[] = {'-', '.', '/', '_', '<', '>', '&', '|'};
  if (isalpha(c) || isdigit(c))
    result = 1;
  else {
    for (int i = 0; i < 8; i++) {
      if (c == otherValids[i]) {
        result = 1;
        break;
      }
    }
  }
  return result;
}


/*
 * reads a single line from terminal and parses it into an array of
 * tokens/words by splitting the line on spaces.  Adds NULL as final token
 */
char** readLineOfWords() {

  // allocate memory for array of array of characters (list of words)
  char** words = (char**) malloc( MAX_NUM_WORDS * sizeof(char*) );
  int i;
  for (i = 0; i < MAX_NUM_WORDS; i++) {
    words[i] = (char*) malloc( MAX_WORD_LENGTH );
  }
  // read actual line of input from terminal
  int bytes_read;
  char *buf;
  buf = (char*) malloc( MAX_WORD_LENGTH + 1 );
  bytes_read = getline(&buf, &MAX_WORD_LENGTH, stdin);
  if (bytes_read > 100) {
    words[0] = "-1";
    return words;
  } else if (bytes_read == -1) {
    // this is EOF (ctrl-D)
    printf("logout\n[Process completed]\n"); fflush(stdout);
    exit(0);
  }

  // take each word from line and add it to next spot in list of words
  i = 0;
  char* word = (char*) malloc( MAX_WORD_LENGTH );
  word = strtok(buf, " \n");
  while (word != NULL && i < MAX_NUM_WORDS) {
    strcpy(words[i++], word);
    word = strtok(NULL, " \n");
  }


  if (i >= MAX_NUM_WORDS) {
    printf( "WARNING: line contains more than %d words!\n", (int)MAX_NUM_WORDS );
  }
  else
    words[i] = NULL;

  // return the list of words
  return words;

}

