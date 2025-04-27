/* ICCS227: Project 1: icsh
 * Name: Cherlyn Wijitthadarat
 * StudentID: 6480330
 * Tag: 0.1.0
 */

#include "stdio.h"
#include "string.h"
#include "stdlib.h"

#define MAX_CMD_BUFFER 255
#define MAX_LINE_LENGTH 255

char** toTokens(char* buffer) {
    char** toReturn = malloc(MAX_CMD_BUFFER * sizeof(char*));
    char* token = strtok(buffer, " \t\n");
    int i = 0;
    while (token != NULL) {
        toReturn[i] = token;
        token = strtok(NULL, " \t\n");
        i++;
    }
    toReturn[i] = NULL;
    return toReturn;
}

char** copyTokens(char** tokens) {
    char** toReturn = malloc(MAX_CMD_BUFFER * sizeof(char*));
    int i = 0;
    while (tokens[i] != NULL) {
        int len = strlen(tokens[i]);
        toReturn[i] = malloc((len + 1) * sizeof(char));
        strcpy(toReturn[i], tokens[i]);
        i++;
    }
    toReturn[i] = NULL;
    return toReturn;
}

void printToken(char** token, int start) {
    for (int i = start; token[i] != NULL; i++) {
        printf("%s ", token[i]);
    }
    printf("\n");
}

void command(char** curr, char** prev) {
    /* Turns prev to str */
    char* prev_output = calloc(MAX_LINE_LENGTH, sizeof(char));
    strcpy(prev_output, "");
    for (int i = 1; prev[i] != NULL; i++) {
        strcat(prev_output, prev[i]);
        strcat(prev_output, " ");
    }

    /* !! */
    if (strcmp(curr[0], "!!") == 0 && curr[1] == NULL) {
        if (strcmp(prev_output, "") != 0) {
            printf("%s %s\n", prev[0], prev_output);
            printf("%s\n", prev_output);
        } else {
            printf("No previous output\n");
        }
    } else {
        /* echo */
        if (strcmp(curr[0], "echo") == 0 && curr[1] != NULL) {
            printToken(curr, 1);
        /* exit */
        } else if (strcmp(curr[0], "exit") == 0 && curr[2] == NULL) {
            printf("bye\n");
            int n = atoi(curr[1]);
            if (n > 255) {
                n = n & 0xFF;
            }
            exit(n);
	} else if (strcmp(curr[0], "exit") == 0 && curr[1] == NULL) {
            printf("bye\n");
            exit(0);
        } else {
            printf("bad command\n");
        }
    }

    free(prev_output);
}

int main() {
    char buffer[MAX_CMD_BUFFER];
    char** prevBuffer = toTokens(buffer); 
    printf("Starting IC shell\n");

    while (1) {
        printf("icsh $ ");
        fgets(buffer, MAX_CMD_BUFFER, stdin);
	buffer[strcspn(buffer, "\n")] = '\0';
	if (strlen(buffer) == 0) { continue;}
        char** currBuffer = toTokens(buffer); 
        command(currBuffer, prevBuffer);
        prevBuffer = copyTokens(currBuffer);
        free(currBuffer);
    }
    return 0;
}
