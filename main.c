#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "maze.h"
#include "expression.h"

/* system-error channels; MazeResult and ExpressionResult cannot carry them */
extern const char *maze_error(void);
extern const char *expr_error(void);

static void usage(const char *prog) {
    fprintf(stderr,"usage: %s A <maze_file>\n",prog);
    fprintf(stderr,"       %s B <expression_file>\n",prog);
    fprintf(stderr,"       %s C <maze_file> <expressions_dir>\n",prog);
    exit(EXIT_FAILURE);
}

static int run_maze(MazeResult res,const char *path) {
    if(maze_error())
    {
        fprintf(stderr,"error: %s: %s\n",path,maze_error());
        free(res.path);
        return EXIT_FAILURE;
    }
    if(res.status==MAZE_SOLVABLE)
        printf("SOLVABLE\nPATH: %s\n",res.path);
    else if(res.status==MAZE_UNSOLVABLE)
        printf("UNSOLVABLE\n");
    else
        printf("INVALID_MAZE\n");
    free(res.path);
    return EXIT_SUCCESS;
}

static int run_expr(const char *path) {
    ExpressionResult res=evaluate_expression_file(path);

    if(expr_error())
    {
        fprintf(stderr,"error: %s: %s\n",path,expr_error());
        free(res.result_str);
        return EXIT_FAILURE;
    }
    if(res.status==EXPR_VALID)
        printf("RESULT: %s\n",res.result_str);
    else
        printf("INVALID_EXPRESSION\n");
    free(res.result_str);
    return EXIT_SUCCESS;
}

int main(int argc,char **argv) {
    const char *prog=argc?argv[0]:"maze_solver";

    if(argc<2)
        usage(prog);
    if(!strcmp(argv[1],"A"))
    {
        if(argc!=3)
            usage(prog);
        return run_maze(check_maze(argv[2]),argv[2]);
    }
    if(!strcmp(argv[1],"B"))
    {
        if(argc!=3)
            usage(prog);
        return run_expr(argv[2]);
    }
    if(!strcmp(argv[1],"C"))
    {
        if(argc!=4)
            usage(prog);
        return run_maze(check_extended_maze(argv[2],argv[3]),argv[2]);
    }
    usage(prog);
    return EXIT_FAILURE;
}
