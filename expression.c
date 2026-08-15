#include <stdlib.h>
#include "expression.h"

static const char *sys_err=NULL;

const char *expr_error(void) {
    return sys_err;
}

/* TODO part B: the parser and evaluator are not implemented yet */
ExpressionResult evaluate_expression_file(const char *file_path) {
    ExpressionResult res={EXPR_INVALID,NULL};

    (void)file_path;
    sys_err=NULL;
    return res;
}
