/*
 *  csd5127: George Kiosklis
 *  Shell implementation 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>

#define MAX_LINE 4096     /* Maximum line length for input */
#define MAX_ARGS 128      /* Maximum number of command arguments */
#define MAX_VARS 128      /* Maximum number of shell variables */
#define MAX_VAR_NAME 64   /* Maximum variable name length */
#define MAX_VAR_VALUE 512 /* Maximum variable value length */
#define MAX_PIPES 32      /* Maximum number of pipes in a pipeline */

/* structure gia thn apo8hkeysh name-value pairs */
typedef struct
{
    char name[MAX_VAR_NAME];
    char value[MAX_VAR_VALUE];
} Var;



/*
 * Custom strdup function
 * h kanonikh strdup den etrexe sto qemu opote eftiaxa custom 
 */
char *my_strdup(const char *s)
{
    char *copy = malloc(strlen(s) + 1);
    return copy ? strcpy(copy, s) : NULL;
}




/*
 * Display the shell prompt showing
 */
void display_shell(void)
{
    char cwd[MAX_LINE];
    char *username=getlogin();
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        strcpy(cwd, "uknown");
    }
    if (username == NULL)
    {
        username="user";
    }
    printf("%s@-5127-hy345sh:%s$ ", username, cwd);
    fflush(stdout);
}

/* Array gia thn apo8hkeysh global variables kai counter */
Var variable[MAX_VARS];
int var_count = 0;

/* Track exit status of last command for if statement conditions */
int last_exit_status = 0;

/*
 * Anakthsh enos value apo ena shell variable by name
 * Returns: variable value string alliws NULL an den vre8ei
 */
char *get_Var(const char *name)
{
    for (int i = 0; i < var_count; i++)
    {
        if (strcmp(variable[i].name, name) == 0)
        {
            return variable[i].value;
        }
    }
    return NULL;
}



/*
 * Set or update a shell variable
 * An to variable yparxei kanei update to value alliws dhmiourgei neo variable
 */
void set_var(const char *name, const char *value)
{
    for (int i = 0; i < var_count; i++)
    {
        if (strcmp(variable[i].name, name) == 0)
        {
            strncpy(variable[i].value, value, MAX_VAR_VALUE - 1);
            variable[i].value[MAX_VAR_VALUE - 1]='\0';
            return;
        }
    }
    if (var_count < MAX_VARS)
    {
        strncpy(variable[var_count].name, name, MAX_VAR_NAME - 1);
        variable[var_count].name[MAX_VAR_NAME - 1]='\0';
        strncpy(variable[var_count].value, value, MAX_VAR_VALUE - 1);
        variable[var_count].value[MAX_VAR_VALUE - 1]='\0';
        var_count++;
    }
}

/*
 * Kanei expand ta variables se ena command string
 * Antika8ista $VAR me ta values tou viriable (supports alphanumeric and underscore)
 * Returns: static buffer me ena expanded string
 */


char *var_expansion(const char *input)
{
    static char result[MAX_LINE];
    char var_name[MAX_VAR_NAME];
    int i=0, j=0;
    while (input[i] != '\0' && j < MAX_LINE - 1)
    {
        if (input[i] == '$')
        {
            i++;
            int k = 0;
            while (input[i] != '\0' && (isalnum(input[i]) || input[i] == '_') && k < MAX_VAR_NAME - 1)
            {
                var_name[k++]=input[i++];
            }
            var_name[k]='\0';
            char *val=get_Var(var_name);
            if (val != NULL)
            {
                int len=strlen(val);
                if (j + len < MAX_LINE)
                {
                    strcpy(&result[j], val);
                    j+=len;
                }
            }
        }
        else
        {
            result[j++]=input[i++];
        }
    }
    result[j]='\0';
    return result;
}




/*
 * Kanei check an yparxei word pou einai shell control flow keyword
 * Returns: 1 an einai keyword 0 alliws
 */
int check_keyword(const char *key)
{
    return (strcmp(key, "if") == 0 || strcmp(key, "for") == 0 || strcmp(key, "then") == 0 || strcmp(key, "do") == 0 || strcmp(key, "fi") == 0 || strcmp(key, "done") == 0);
}

/*
 * Diavazei multiline input gia ta control structures (if/for)
 * Synexizei na diavazei mexris otou vre8ei matching closing keyword (fi/done)
 * Mporei kai xeirizetai nested control structures kanontas track to depth
 * Returns: static buffer me accumulated lines
 */
char *read_multiline_cmd(const char *key)
{
    static char buffer[MAX_LINE * 10];
    char line[MAX_LINE];
    buffer[0]='\0';

    const char *end_key = (strcmp(key, "if") == 0) ? "fi" : "done";
    int depth = 1;
    while (depth > 0)
    {
        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            break;
        }
        strcat(buffer, line);
        if (strstr(line, key) != NULL)
        {
            depth++;
        }
        if (strstr(line, end_key) != NULL)
        {
            depth--;
        }
    }
    return buffer;
}



void parse_and_exec(char *line);

/*
 * Executes a single command
 * Handles: Variable assignments (VAR=value), Variable expansion ($VAR), I/O redirection (<, >, >>),
 * Built-in commands (cd, exit), External commands via fork/exec
 */
void execute_cmd(char *cmd)
{
    char *args[MAX_ARGS];
    int argc=0;
    char *token;
    char *input_file=NULL;
    char *output_file=NULL;
    int append=0;

    /* Kanei check gia variable assignment (x=value or x="value with spaces") */
    char *eq=strchr(cmd, '=');
    if (eq != NULL && eq != cmd)
    { /* Exei (=) kai den einai sthn arxh */
        /* check an einai aplo assignment (xwris special char prin to (=)) */
        int is_assignment=1;
        for (char *p = cmd; p < eq; p++)
        {
            if (*p == ' ' || *p == '>' || *p == '<' || *p == '|')
            {
                is_assignment=0;
                break;
            }
        }

        if (is_assignment)
        {
            /* einai variable assignment */
            *eq='\0';
            char *name=cmd;
            char *val=eq + 1;

            /* afairei ta spaces gia to name */
            while (*name == ' ' || *name == '\t')
                name++;

            /* afairei quotes an yparxoun */
            if (val[0] == '"' && val[strlen(val) - 1] == '"')
            {
                val[strlen(val) - 1]='\0';
                val++;
            }

            set_var(name, val);
            last_exit_status = 0; /* Assignment always succeeds */
            return; /* Exit afotou kanei set to variable */
        }
    }

    char expanded[MAX_LINE];
    strncpy(expanded, var_expansion(cmd), MAX_LINE - 1);
    expanded[MAX_LINE - 1]='\0';

    token = strtok(expanded, " \t\n");
    while (token != NULL && argc < MAX_ARGS - 1)
    {
        if (strcmp(token, "<") == 0)
        {
            token=strtok(NULL, " \t\n");
            if (token != NULL)
            {
                input_file=token;
            }
        }
        else if (strcmp(token, ">") == 0)
        {
            token=strtok(NULL, " \t\n");
            if (token != NULL)
            {
                output_file=token;
                append=0; /* Overwrite */
            }
        }
        else if (strcmp(token, ">>") == 0)
        {
            token=strtok(NULL, " \t\n");
            if (token != NULL)
            {
                output_file=token;
                append=1; /* Append */
            }
        }
        else
        {
            /* check gia embedded redirection opws: cat<file, ls>out */
            char *redir=strchr(token, '>');
            if (redir != NULL)
            {
                if (redir[1] == '>')
                {
                    /* >> embedded */
                    *redir='\0';
                    if (strlen(token) > 0)
                    {
                        args[argc++]=token;
                    }
                    output_file=redir + 2;
                    append = 1;
                }
                else
                {
                    /* > embedded */
                    *redir='\0';
                    if (strlen(token) > 0)
                    {
                        args[argc++] = token;
                    }
                    output_file=redir + 1;
                    append=0;
                }
            }
            else
            {
                redir = strchr(token, '<');
                if (redir != NULL)
                {
                    /* < embedded (p.x. cat<file) */
                    *redir='\0';
                    if (strlen(token) > 0)
                    {
                        args[argc++] = token;
                    }
                    input_file = redir + 1;
                }
                else
                {
                    /* Aplo argument */
                    args[argc++]=token;
                }
            }
        }
        token=strtok(NULL, " \t\n");
    }
    args[argc] = NULL;
    if (argc==0) return;
    if (strcmp(args[0], "cd") == 0)
    {
        if (argc > 1)
        {
            if (chdir(args[1]) != 0)
            {
                perror("cd");
            }
        }
        else
        {
            char *home=getenv("HOME");
            if (home==NULL)
            {
                fprintf(stderr, "cd: HOME not set\n");
            }
            else
            {
                if (chdir(home) != 0)
                {
                    perror("cd");
                }
            }
        }
        return;
    }




    /* Exit shell */
    if (strcmp(args[0], "exit") == 0)
    {
        printf("Terminating shell...\n");
        printf("Goodbye!\n");
        exit(0);
    }

    /* External command: fork and exec */
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return;
    }
    else if (pid == 0)
    {
        /* Child process */

        /* input redirection */
        if (input_file!=NULL)
        {
            int fd=open(input_file, O_RDONLY);
            if (fd < 0)
            {
                perror("open input");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        /* output redirection */
        if (output_file!=NULL)
        {
            int flags=O_WRONLY | O_CREAT;
            flags|=append ? O_APPEND : O_TRUNC;
            int fd=open(output_file, flags, 0644);
            if (fd < 0)
            {
                perror("open output");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        execvp(args[0], args);
        perror("execvp");
        exit(1);
    }
    else
    {
        /* Parent process */
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            last_exit_status = WEXITSTATUS(status);
        }
    }
}



/*
 * xirismos twn command pipelines p.x. (cmd1 | cmd2 | cmd3 | ...)
 * Dimiourgei pipes gia na kanei connect to stdout apo ena command sto stdin tou epomenou
 * Kanei fork ta child processes gia ka8e stage sto pipeline
 * Ypostirizei mexri MAX_PIPES taftoxrona pipeline stages
 */
void pipelining(char *cmd)
{
    char *commands[MAX_PIPES];
    int cmd_c=0;

    /* kanei ena copy wste na diathrisoume to arxiko */
    char cmd_copy[MAX_LINE * 2];
    strncpy(cmd_copy, cmd, MAX_LINE * 2 - 1);
    cmd_copy[MAX_LINE * 2 - 1] = '\0';

    /* kanei split mesw tou pipe */
    char *token=strtok(cmd_copy, "|");
    while (token != NULL && cmd_c < MAX_PIPES)
    {
        while (*token == ' ' || *token == '\t')
        {
            token++;
        }
        commands[cmd_c++]=my_strdup(token); /* Xrhsh strdup gia th diathrhsh arxikou */
        token=strtok(NULL, "|");
    }

    if (cmd_c == 1)
    {
        execute_cmd(commands[0]);
        free(commands[0]);
        return;
    }



    int pipes[MAX_PIPES][2];
    for (int i = 0; i < cmd_c - 1; i++)
    {
        if (pipe(pipes[i]) < 0)
        {
            perror("pipe");
            for (int j = 0; j < cmd_c; j++){
                free(commands[j]);
            }
            return;
        }
    }
    for (int i = 0; i < cmd_c; i++)
    {
        pid_t pid=fork();

        if (pid < 0)
        {
            perror("fork");
            for (int j = 0; j < cmd_c; j++)
                free(commands[j]);
            return;
        }
        else if (pid == 0)
        {
            /* Child process */

            /* Redirect input apo prohgoumeno pipe */
            if (i > 0)
            {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }

            /* Redirect to output sto epomeno pipe (MONO AN DEN EINAI TO TELEFTAIO COMMAND) */
            if (i < cmd_c - 1)
            {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            /* Close all pipes */
            for (int j = 0; j < cmd_c - 1; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            /* parse and execute to command me ta redirections tou*/
            /* Swstos xeirismos redirections */
            char *args[MAX_ARGS];
            int argc=0;
            char *input_file=NULL;
            char *output_file=NULL;
            int append=0;
            /* Expand variables */
            char expanded_cmd[MAX_LINE];
            strncpy(expanded_cmd, var_expansion(commands[i]), MAX_LINE - 1);
            expanded_cmd[MAX_LINE - 1] = '\0';
            /* Parse the command */
            char expanded_copy[MAX_LINE];
            strncpy(expanded_copy, expanded_cmd, MAX_LINE - 1);
            expanded_copy[MAX_LINE - 1] = '\0';
            char *t=strtok(expanded_copy, " \t\n");

            while (t != NULL && argc < MAX_ARGS - 1)
            {
                if (strcmp(t, "<") == 0)
                {
                    t = strtok(NULL, " \t\n");
                    if (t != NULL)
                        input_file=t;
                }
                else if (strcmp(t, ">") == 0)
                {
                    t = strtok(NULL, " \t\n");
                    if (t != NULL)
                    {
                        output_file=t;
                        append=0;
                    }
                }
                else if (strcmp(t, ">>") == 0)
                {
                    t = strtok(NULL, " \t\n");
                    if (t != NULL)
                    {
                        output_file=t;
                        append=1;
                    }
                }
                else
                {
                    /* Check for embedded redirection */
                    char *redir = strchr(t, '>');
                    if (redir != NULL)
                    {
                        if (redir[1] == '>')
                        {
                            *redir='\0';
                            if (strlen(t) > 0)
                                args[argc++]=t;
                            output_file=redir+2;
                            append=1;
                        }
                        else
                        {
                            *redir = '\0';
                            if (strlen(t) > 0){
                                args[argc++] = t;
                            }
                            output_file=redir + 1;
                            append=0;
                        }
                    }
                    else
                    {
                        redir=strchr(t, '<');
                        if (redir != NULL)
                        {
                            *redir='\0';
                            if (strlen(t) > 0){
                                args[argc++] = t;
                            }
                            input_file=redir + 1;
                        }
                        else
                        {
                            args[argc++]=t;
                        }
                    }
                }
                t=strtok(NULL, " \t\n");
            }
            args[argc]=NULL;

            /* efarmogh input redirection an xreiazetai */
            if (input_file != NULL)
            {
                int fd = open(input_file, O_RDONLY);
                if (fd < 0)
                {
                    perror("open input");
                    exit(1);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            /* efarmogh output redirection an xreiazetai */
            if (output_file != NULL)
            {
                int flags=O_WRONLY | O_CREAT;
                flags|=append ? O_APPEND : O_TRUNC;
                int fd=open(output_file, flags, 0644);
                if (fd < 0)
                {
                    perror("open output");
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }



            /* Execute command */
            if (argc > 0)
            {
                execvp(args[0], args);
                perror("execvp");
            }
            exit(1);
        }
    }

    /* Parent kanei close ola ta pipes */
    for (int i = 0; i < cmd_c - 1; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    /* Wait gia ola ta children */
    for (int i = 0; i < cmd_c; i++)
    {
        wait(NULL);
    }
    /* Free allocated memory */
    for (int i = 0; i < cmd_c; i++)
    {
        free(commands[i]);
    }
}







/*
 * Ektelesh tou if-then-fi statement
 * Syntax: if CONDITION; then BODY; fi
 * Kanei fork gia na ektimisei to condition, ektelei to body an to exit status einai 0
 */
void if_statement(char *line)
{
    char condition[MAX_LINE];
    char body[MAX_LINE * 5];
    char *then=strstr(line, "then");

    if (then == NULL)
    {
        fprintf(stderr, "Syntax error: 'then' expected\n");
        return;
    }

    int con_len=then - (line + 2);
    strncpy(condition, line + 2, con_len);
    condition[con_len]='\0';
    char *con_start=condition;

    while (*con_start == ' ' || *con_start == '\t' || *con_start == '\n')
    {
        con_start++;
    }
    char *fi=strstr(then, "fi");
    if (fi == NULL)
    {
        fprintf(stderr, "Syntax error: 'fi' expected\n");
        return;
    }

    int body_len=fi-(then + 4);
    strncpy(body, then + 4, body_len);
    body[body_len]='\0';

    /* Execute condition and check exit status */
    parse_and_exec(con_start);

    if (last_exit_status == 0)
    {
        parse_and_exec(body);
    }
}

/*
 * Ektelesh for loop
 * Syntax: for VAR in VALUE1 VALUE2 ...; do BODY; done
 * Epanaliptikh diadikasia se space-separated values, kanontas set to VAR se ka8e value
 */
void for_loop(char *line)
{
    char var_name[MAX_VAR_NAME];
    char *tokens[MAX_ARGS];
    int token_c = 0;
    char body[MAX_LINE * 5];
    char *in = strstr(line, " in ");
    if (in == NULL)
    {
        fprintf(stderr, "Syntax error: 'in' expected\n");
        return;
    }

    int var_len=in-(line + 4);
    strncpy(var_name, line + 4, var_len);
    var_name[var_len]='\0';
    char *var_start=var_name;
    while (*var_start == ' ' || *var_start == '\t')
    {
        var_start++;
    }
    char *var_end=var_start+strlen(var_start) - 1;
    while (var_end > var_start && (*var_end == ' ' || *var_end == '\t'))
    {
        var_end--;
    }
    *(var_end + 1)='\0';
    char *do_pos=strstr(in, "do");
    if (do_pos == NULL)
    {
        fprintf(stderr, "Syntax error: 'do' expected\n");
        return;
    }

    char token_str[MAX_LINE];
    int tok_len=do_pos - (in + 4);
    strncpy(token_str, in + 4, tok_len);
    token_str[tok_len]='\0';
    char *expanded_tok=var_expansion(token_str);
    char *t=strtok(expanded_tok, " \t\n;");

    while (t != NULL && token_c < MAX_ARGS)
    {
        if (t[0] == '"')
        {
            t++;
            char *end_q = strchr(t, '"');
            if (end_q)
            {
                *end_q='\0';
            }
        }
        tokens[token_c++] = my_strdup(t);
        t = strtok(NULL, " \t\n;");
    }
    char *done=strstr(do_pos, "done");
    if (done == NULL)
    {
        fprintf(stderr, "Syntax error: 'done' expected\n");
        return;
    }

    int body_len=done-(do_pos + 2);
    strncpy(body, do_pos + 2, body_len);
    body[body_len]='\0';
    for (int i = 0; i < token_c; i++)
    {
        set_var(var_start, tokens[i]);
        parse_and_exec(body);
        free(tokens[i]);
    }
}

/*
 * Parse and execute command line input
 * Parexei:
 * Control flow structures (if/for)
 * Command chaining with semicolons (;)
 * Pipeline detection (|)
 * Variable assignments
 * Kanei split to input sta semicolons (respecting quotes kai control structures)
 * Dromologei ta commands stous katalhlous handlers
 */
void parse_and_exec(char *line)
{
    while (*line == ' ' || *line == '\t' || *line == '\n')
    {
        line++;
    }
    if (strlen(line) == 0)
    {
        return;
    }
    if (strncmp(line, "if", 3) == 0 || strncmp(line, "if[", 3) == 0)
    {
        if_statement(line);
        return;
    }
    if (strncmp(line, "for", 4) == 0)
    {
        for_loop(line);
        return;
    }

    /* Manual semicolon splitting that respects quotes AND control structures */
    char *commands[MAX_ARGS];
    int cmd_count=0;
    char *start=line;
    int in_quotes=0;
    int control_depth=0; /* Track if/for nesting */
    for (char *p = line; *p != '\0'; p++)
    {
        if (*p == '"')
        {
            in_quotes = !in_quotes;
        }
        /* Track control structure keywords (mono otan den einai se quotes) */
        if (!in_quotes)
        {
            /* Check gia "if" keyword */
            if (p == line || *(p - 1) == ' ' || *(p - 1) == ';')
            {
                if (strncmp(p, "if ", 3) == 0 || strncmp(p, "if[", 3) == 0)
                {
                    control_depth++;
                }
                else if (strncmp(p, "for ", 4) == 0)
                {
                    control_depth++;
                }
                else if (strncmp(p, "fi", 2) == 0 && (p[2] == '\0' || p[2] == ' ' || p[2] == ';'))
                {
                    control_depth--;
                }
                else if (strncmp(p, "done", 4) == 0 && (p[4] == '\0' || p[4] == ' ' || p[4] == ';'))
                {
                    control_depth--;
                }
            }
        }

        /* Split mono se (;) an den einai se quotes kai den einai mesa se control structure */
        if (*p == ';' && !in_quotes && control_depth == 0 && cmd_count < MAX_ARGS)
        {
            *p='\0';
            commands[cmd_count++]=start;
            start=p+1;
        }
    }

    /* Don't forget the last command */
    if (*start != '\0' && cmd_count < MAX_ARGS)
    {
        commands[cmd_count++]=start;
    }

    /* execute to ka8e command */
    for (int i = 0; i < cmd_count; i++)
    {
        char *cmd=commands[i];

        /* Trim spaces */
        while (*cmd == ' ' || *cmd == '\t' || *cmd == '\n')
        {
            cmd++;
        }

        if (strlen(cmd) > 0)
        {
            /* Check an xekinaei kapoio control structure */
            if (strncmp(cmd, "if", 2) == 0 || strncmp(cmd, "if[", 3) == 0)
            {
                if_statement(cmd);
                continue;
            }
            if (strncmp(cmd, "for", 3) == 0)
            {
                for_loop(cmd);
                continue;
            }

            /* Check an einai variable assignment */
            int is_var_assignment=0;
            char *eq = strchr(cmd, '=');
            if (eq != NULL)
            {
                int has_space_before=0;
                for (char *p = cmd; p < eq; p++)
                {
                    if (*p == ' ')
                    {
                        has_space_before=1;
                        break;
                    }
                }



                int has_special_before=0;
                for (char *p = cmd; p < eq; p++)
                {
                    if (*p == '>' || *p == '<' || *p == '|')
                    {
                        has_special_before=1;
                        break;
                    }
                }
                if (!has_space_before && !has_special_before)
                {
                    is_var_assignment=1;
                }
            }
            /* Execute command */
            if (is_var_assignment)
            {
                execute_cmd(cmd);
            }
            else if (strchr(cmd, '|') != NULL)
            {
                pipelining(cmd);
            }
            else
            {
                execute_cmd(cmd);
            }
        }
    }
}

/*
 * Main shell loop
 * Displays prompt
 * Diavazei to user input
 * Xeirizetai multiline control structures
 * Proothei commands ston parser
 */
int main()
{
    char line[MAX_LINE];
    printf("Shell initialized.\n");
    printf("Welcome to my hy345shell...\n");
    printf("Type 'exit' to terminate.\n");
    while (1)
    {
        display_shell();
        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            break;
        }
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0)
        {
            continue;
        }
        char first_word[64];
        sscanf(line, "%s", first_word);
        if (strcmp(first_word, "if") == 0 && strstr(line, "fi") == NULL)
        {
            char *full_cmd = read_multiline_cmd("if");
            char combined[MAX_LINE * 10];
            strcpy(combined, line);
            strcat(combined, " ");
            strcat(combined, full_cmd);
            parse_and_exec(combined);
        }
        else if (strcmp(first_word, "for") == 0 && strstr(line, "done") == NULL)
        {
            char *full_cmd=read_multiline_cmd("for");
            char combined[MAX_LINE * 10];
            strcpy(combined, line);
            strcat(combined, " ");
            strcat(combined, full_cmd);
            parse_and_exec(combined);
        }
        else
        {
            parse_and_exec(line);
        }
    }
    return 0;
}
