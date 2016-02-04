#include <ctype.h>
#include <errno.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"
#define min(a,b) a<b?a:b

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

/* Path of current directory */
char curr_dir[PATH_MAX];

/* Initial path */
char init_dir[PATH_MAX];

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_wait(struct tokens *tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_pwd,  "pwd", "print current working directory"},
  {cmd_cd,   "cd", "change working directory"},
  {cmd_wait, "wait", "wait for all background jobs to finish"},
};

void print_error(const char *msg, const int num, ...) {
  fprintf(stderr, "-shell: %s", msg);
  if (num) {
    va_list valist;
    va_start(valist, num);
    for (int i = 0; i < num; ++i)
      fprintf(stderr, ": %s", va_arg(valist, const char *));
    va_end(valist);
  }
  fprintf(stderr, "\n");
}

/* Prints a helpful description for the given command */
int cmd_help(struct tokens *tokens) {
  for (int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(struct tokens *tokens) {
  exit(0);
}

/* Print current working directory */
int cmd_pwd(struct tokens *tokens) {
  if (strlen(curr_dir))
    fprintf(stdout, "%s\n", curr_dir);
  else
    fprintf(stdout, "/\n");
  return 0;
}

/* Wait for all background jobs to finish */
int cmd_wait(struct tokens *tokens) {
  while (wait(NULL) > 0);
  return 0;
}

void trunc_last_dir(char *path) {
  if (path == NULL)
    return;
  int len = strlen(path);
  for (int i = len - 1; i >= 0; --i)
    if (path[i] == '/') {
      path[i] = 0;
      break;
    }
}

int has_slash(const char *path) {
  while (*path != 0 && *path != '/')
    ++path;
  return *path == '/';
}

void mov_path(char *curr_dir_tmp, char *new_dir) {
  char *new_dir_end = new_dir + strlen(new_dir);
  if (new_dir[0] == '/') {
    curr_dir_tmp[0] = 0;
    ++new_dir;
  }
  while (new_dir < new_dir_end) {
    char *first_slash = new_dir;
    while (first_slash != new_dir_end && *first_slash != '/') ++first_slash;
    *first_slash = 0;
    // fprintf(stdout, "DEBUG: new_dir: %s\n", new_dir);
    int len = strlen(new_dir);
    if (!len)
      goto next_sec;
    if (new_dir[0] == '.') {
      if (len == 1)
        goto next_sec;
      if (new_dir[1] == '.' && len == 2) {
        trunc_last_dir(curr_dir_tmp);
        goto next_sec;
      }
    }
    sprintf(curr_dir_tmp, "%s/%s", curr_dir_tmp, new_dir);
    next_sec:
      new_dir = first_slash + 1;
  }
}

/* Change working directory */
int cmd_cd(struct tokens *tokens) {
  struct stat s;
  char *new_dir = tokens_get_token(tokens, 1);
  if (new_dir) {
    char curr_dir_tmp[sizeof(curr_dir)];
    memcpy(curr_dir_tmp, curr_dir, sizeof(curr_dir));
    mov_path(curr_dir_tmp, new_dir);
    if (stat(curr_dir_tmp, &s) == 0) {
      if (!S_ISDIR(s.st_mode)) {
        print_error("cd", 2, curr_dir_tmp, "Not a directory");
        return 1;
      }
    } else {
      print_error("cd", 2, curr_dir_tmp, "No such file or directory");
      return 1;
    }
    // fprintf(stdout, "DEBUG: curr_dir_tmp: %s\n", curr_dir_tmp);
    memcpy(curr_dir, curr_dir_tmp, sizeof(curr_dir));
  } else {
    memcpy(curr_dir, init_dir, sizeof(curr_dir));
  }
  return 0;
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

int main(int argc, char *argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  if (!getcwd(curr_dir, sizeof(curr_dir))) {
    print_error("Cannot get current working directory.", 0);
    return 1;
  }
  memcpy(init_dir, curr_dir, sizeof(curr_dir));

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      /* REPLACE this to run commands as programs. */
      // fprintf(stdout, "This shell doesn't know how to run programs.\n");
      char *path_to_prog = tokens_get_token(tokens, 0);
      if (path_to_prog != NULL && strlen(path_to_prog)) {
        int n_arg = tokens_get_length(tokens);
        char *last_token = tokens_get_token(tokens, n_arg-1);
        int pid = fork();
        if (pid) {
          if (strcmp(last_token, "&"))
            waitpid(pid, NULL, 0);
        } else {
          setpgrp();
          if (strcmp(last_token, "&") == 0)
            --n_arg;
          char **arg_list = malloc((n_arg + 1) * sizeof(char*));
          for (int i = 0; i < n_arg; ++i) {
            arg_list[i] = tokens_get_token(tokens, i);
            // printf("DEBUG: arg: %s\n", arg_list[i]);
            if (strcmp(arg_list[i], ">") == 0) {
              if (n_arg == i+1) {
                print_error("No output file specified.", 0);
                return -1;
              }
              n_arg = i;
              if (freopen(tokens_get_token(tokens, i+1), "w", stdout) == NULL) {
                print_error("Cannot open output file.", 0);
                return -1;
              }
            } else if (strcmp(arg_list[i], "<") == 0) {
              if (n_arg == i+1) {
                print_error("No input file specified.", 0);
                return -1;
              }
              n_arg = i;
              if (freopen(tokens_get_token(tokens, i+1), "r", stdin) == NULL) {
                print_error("Cannot open input file.", 0);
                return -1;
              }
            }
          }
          arg_list[n_arg] = NULL;
          if (has_slash(path_to_prog)) {
            mov_path(curr_dir, path_to_prog);
            execv(curr_dir, arg_list);
          } else {
            char *env_path = getenv("PATH");
            char *env_path_end = env_path + strlen(env_path);
            while (env_path < env_path_end) {
              char *det = env_path;
              while (det != env_path_end && *det != ':') ++det;
              *det = 0;
              strcpy(curr_dir, env_path);
              int single_path_len = strlen(curr_dir);
              if (curr_dir[single_path_len - 1] != '/')
                curr_dir[single_path_len++] = '/';
              strcpy(curr_dir + single_path_len, path_to_prog);
              execv(curr_dir, arg_list);
              env_path = det + 1;
            }
          }
          print_error(path_to_prog, 1, "cannot execute");
          return -1;
        }
      }
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
