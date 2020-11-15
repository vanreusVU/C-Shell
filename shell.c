#include "parser/ast.h"
#include "sys/wait.h"
#include "shell.h"
#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include "stdlib.h"
#include <stdio.h> 
#include <signal.h> 
#include <limits.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

void handle_sigint(int sig) 
{ 
    printf("Signal= %d\n", sig); 
} 

void handle_sigstp(int sig) 
{ 
    exit(sig);
}

void initialize(void)
{
    signal(SIGINT, handle_sigint); 
    signal(SIGTSTP, handle_sigstp); 
    
    //char* username = getenv("USER");
    if (prompt){
        prompt = "vush$"; 
    }
}

int handle_builtin(node_t *n){
    if(strcmp(n->command.program,"exit") == 0){
        exit(atoi(n->command.argv[1]));
        return 1;
    }
    else if(strcmp(n->command.program,"cd") == 0){
        chdir(n->command.argv[1]);
        return 1;
    }
    else if(strcmp(n->command.program, "set") == 0){
        putenv(n->command.argv[1]); 
        return 1;
    }
    else if(strcmp(n->command.program, "unset") == 0){
        unsetenv(n->command.argv[1]);
        return 1;
    }
    return 0;
}

void execvArg(node_t *n){
    __pid_t pid = fork();

    if(pid == 0){
        if(execvp(n->command.program, n->command.argv) < 0)
            fprintf(stderr, "No such file or directory\n");
    }else{
        waitpid(pid, NULL, 0);
    }
}

void handle_cmd(node_t *n){
    if(handle_builtin(n) == 0)  
        execvArg(n);
}

void handle_pipe(node_t *n){
    int nrPipes = n->pipe.n_parts;
    __pid_t pid1;
    
    for(int i = 0; i < nrPipes; i++){
        if(i < nrPipes - 1){
            int pipefd[2];
            pipe(pipefd);
            pid1 = fork();
            if(pid1 == 0){
                close(pipefd[0]);
                close(STDOUT_FILENO);
                dup(pipefd[1]);
                close(pipefd[1]);

                run_command(n->pipe.parts[i]);
                exit(0);
            }else{
                close(pipefd[1]);
                close(STDIN_FILENO);
                dup(pipefd[0]);
                close(pipefd[0]);
            }
        }else{
            run_command(n->pipe.parts[i]);
        }
    }
}

void handle_redirects(node_t *n){
    int fdTo1;
    __pid_t pid;
    pid = fork();
    if (pid == 0)
    {
        switch (n->redirect.mode){
            case REDIRECT_APPEND: //>>
                fdTo1 = open(n->redirect.target, O_APPEND, 0644);
                 if(fdTo1 < 0){
                    exit(1);
                }
                dup2(fdTo1, n->redirect.fd);
                close(fdTo1);
                break;
            case REDIRECT_DUP: // >&
                fdTo1 = n->redirect.fd2;
                dup2(fdTo1, n->redirect.fd);
                break;
            case REDIRECT_INPUT: // <
                fdTo1 = open(n->redirect.target, O_RDONLY);
                 if(fdTo1 < 0){
                    exit(1);
                }
                dup2(fdTo1, n->redirect.fd);
                close(fdTo1);
                break;
            case REDIRECT_OUTPUT: // >  
                fdTo1 = creat(n->redirect.target, 0644);
                if(fdTo1 < 0){
                    exit(1);
                }
                dup2(fdTo1, n->redirect.fd);
                close(fdTo1);
                //fprintf(stdout,"%d\n",fdTo1);
                //close(n->redirect.fd);
                break;
            default:
                fprintf(stdout,"Not working");
                break;
        }
        run_command(n->redirect.child);
        exit(0);
    }        
    else
    {
        wait(NULL);
    }

    //fprintf(stdout,"--");
}

void handle_subshell(node_t *n){
    __pid_t pid = fork();
    if(pid == 0){
        run_command(n->subshell.child);
        exit(0);
    }
    wait(NULL);
}

void handle_detach(node_t *n){
    __pid_t pid = fork();
    if(pid != 0){
        run_command(n->detach.child);
        exit(0);
    }
    wait(NULL);
}

void run_command(node_t *node)
{
    switch (node->type){
        case NODE_COMMAND:
            handle_cmd(node);
            break;
        case NODE_PIPE:
            handle_pipe(node);
            break;
        case NODE_SEQUENCE:
            run_command(node->sequence.first);
            run_command(node->sequence.second);
            break;
        case NODE_REDIRECT:
            //fprintf(stdout, "%s",node->redirect.child->command.program);
            handle_redirects(node);
            break;
        case NODE_SUBSHELL:
            handle_subshell(node);
            break;
        case NODE_DETACH:
            handle_detach(node);
            break;
        default:
            break;
    }

    if (prompt)
        prompt = "vush$ ";
}