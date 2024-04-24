#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define UNUSED_ATTR __attribute__((unused))

#define BUFSIZE 1024
#define TOKENS_LEN 64
#define TOKEN_DELIM " \t\r\n\a"

typedef enum {
    STATUS_OK,
    STATUS_ERR,
    STATUS_RUNNING = 255,
} Status;

char *csh_getline(void);
char **csh_tokenize(char *line);
Status csh_execute(char **tokens);

int csh_cd(char **tokens);
int csh_help(char **tokens);
int csh_exit(char **tokens);

int main(int argc, char **argv) {
    (void)argc, (void)argv;
    Status status = STATUS_RUNNING;
    char cwd[1024];
    char hostname[1024];
    do {
        if (!getcwd(cwd, sizeof(cwd))) {
            perror("failed to get current working directory");
            return EXIT_FAILURE;
        }
        if (gethostname(hostname, sizeof(hostname)) != 0) {
            perror("failed to get host name");
            return EXIT_FAILURE;
        }
        printf("%s:%s$ ", hostname, cwd);
        char *line = csh_getline();
        if (!line) {
            return EXIT_FAILURE;
        }
        char **tokens = csh_tokenize(line);
        if (!tokens) {
            return EXIT_FAILURE;
        }
        status = csh_execute(tokens);
    } while (status == STATUS_RUNNING);

    return status;
}

char *csh_getline(void) {
    char *line = NULL;
    size_t bufsize = 0;

    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            return NULL;
        } else {
            perror("failed to read line");
            return NULL;
        }
    }

    return line;
}

char **csh_tokenize(char *line) {
    int pos = 0;
    char **tokens = (char **)malloc(sizeof(char *) * BUFSIZE);
    if (!tokens) {
        perror("failed to allocate memory");
        return NULL;
    }
    char *token;

    token = strtok(line, TOKEN_DELIM);
    while (token) {
        tokens[pos++] = token;

        token = strtok(NULL, TOKEN_DELIM);
    }
    tokens[pos] = NULL;

    return tokens;
}

static int csh_launch(char **tokens) {
    pid_t pid, wpid __attribute__((unused));
    int status;

    pid = fork();
    if (pid == 0) {
        // child process
        if (execvp(tokens[0], tokens) == -1) {
            perror("csh");
        }
        return STATUS_ERR;
    } else if (pid < 0) {
        // error forking
        perror("csh");
    } else {
        // parent process
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return STATUS_RUNNING;
}

char *builtin_str[] = {"cd", "help", "exit"};

int (*builtin_func[])(char **) = {&csh_cd, &csh_help, &csh_exit};

int csh_num_builtins() { return sizeof(builtin_str) / sizeof(char *); }

int csh_cd(char **tokens) {
    if (tokens[1] == NULL) {
        fprintf(stderr, "csh: expected argument to `cd`\n");
    } else {
        if (chdir(tokens[1]) != 0) {
            perror("csh");
        }
    }
    return STATUS_RUNNING;
}

int csh_help(char **tokens UNUSED_ATTR) {
    (void)tokens;
    int i;
    printf("The following commands are built in:\n");

    for (i = 0; i < csh_num_builtins(); i++) {
        printf("  %s\n", builtin_str[i]);
    }
    return STATUS_RUNNING;
}

int csh_exit(char **tokens UNUSED_ATTR) {
    (void)tokens;
    return STATUS_OK;
}

Status csh_execute(char **tokens) {
    int i;

    if (tokens[0] == NULL) {
        // an empty command was entered.
        return STATUS_ERR;
    }

    for (i = 0; i < csh_num_builtins(); i++) {
        if (strcmp(tokens[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(tokens);
        }
    }

    return csh_launch(tokens);
}
