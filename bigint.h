#ifndef BIGINT_H
#define BIGINT_H

#include <stddef.h>

/* an unsigned bit pattern of exactly bi_width(x) bits; signedness is a
   property of the evaluation mode, not of the value */
typedef struct BigInt BigInt;

BigInt *bi_zero(size_t width);
BigInt *bi_copy(const BigInt *x);
void bi_free(BigInt *x);

/* the digit strings must already be validated by the lexer; NULL means a
   system error (allocation or size overflow), never an invalid expression */
BigInt *bi_from_dec(const char *s,size_t len);
BigInt *bi_from_bin(const char *s,size_t len);
BigInt *bi_from_hex(const char *s,size_t len);

char *bi_to_dec(const BigInt *x,int is_signed);

size_t bi_width(const BigInt *x);
size_t bi_min_bits(const BigInt *x);
int bi_is_zero(const BigInt *x);
int bi_top_bit(const BigInt *x);

#endif
