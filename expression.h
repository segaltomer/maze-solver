#ifndef EXPRESSION_H
#define EXPRESSION_H

#define BONUS_DIV_MOD_IMPLEMENTED 0

typedef enum {
    EXPR_VALID,
    EXPR_INVALID
} ExpressionStatus;

typedef struct {
    ExpressionStatus status;
    char *result_str; /* Computed value in decimal notation */
} ExpressionResult;

ExpressionResult evaluate_expression_file(const char *file_path);

#endif
