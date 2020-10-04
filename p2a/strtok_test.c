#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
  char input[100] = " leading space";
  char *first = strtok(input, "|");
  printf("\"%s\" - first piece\n", first);
  printf("\"%s\" - input\n", input);
  if (first == NULL) {
    printf("%s did not contain |\n", input);
  }
  
  first = strtok(input, " ");
  printf("first piece: %s\n", first);
  printf("input: %s\n", input);
  char *third = strtok(NULL, " ");
  printf("third piece: %s\n", third);
  printf("fourth piece: %s\n", input);
  
  char whitespace[100] = "    ";
  printf("\"%s\" - original\n", whitespace);
  char *whitespaceTest = strtok(whitespace, " ");
  if (whitespaceTest == NULL) {
    printf("\"%s\" - strtok returned NULL\n", whitespace);
  }
  printf("\"%s\"\n", whitespaceTest);
  
  printf("\n\n");
  char whitespace2[100] = "    ";
  printf("\"%s\" - original whitespace2\n", whitespace2);
  char *whitespaceTest2 = strtok(whitespace, "abcdefg");
}