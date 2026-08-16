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

/* 0 when a value of this width could never be allocated */
int bi_width_supported(size_t width);

size_t bi_width(const BigInt *x);
size_t bi_min_bits(const BigInt *x);
int bi_is_zero(const BigInt *x);
int bi_top_bit(const BigInt *x);
int bi_cmp(const BigInt *a,const BigInt *b);

/* width policy: fixed modes truncate to N, UNLIMITED uses min_bits+1 */
BigInt *bi_truncate(const BigInt *x,size_t n);
BigInt *bi_zext(const BigInt *x,size_t n);
BigInt *bi_sext(const BigInt *x,size_t n);
BigInt *bi_norm_unl(const BigInt *x);

/* 0 when the value does not fit a size_t; compare against the width first */
int bi_to_size(const BigInt *x,size_t *out);

BigInt *bi_not(const BigInt *x);
BigInt *bi_and(const BigInt *a,const BigInt *b);
BigInt *bi_or(const BigInt *a,const BigInt *b);
BigInt *bi_xor(const BigInt *a,const BigInt *b);
BigInt *bi_shl(const BigInt *x,size_t k);
BigInt *bi_shr_log(const BigInt *x,size_t k);
BigInt *bi_shr_arith(const BigInt *x,size_t k);

BigInt *bi_add(const BigInt *a,const BigInt *b);
BigInt *bi_sub_exact(const BigInt *a,const BigInt *b);
BigInt *bi_sub_wrap(const BigInt *a,const BigInt *b,size_t n);
BigInt *bi_mul(const BigInt *a,const BigInt *b);

/* truncating division; remainder takes the sign of a. result width is a's
   width. the caller must reject b==0 first, that is EXPR_INVALID, not a
   system error */
BigInt *bi_divmod(const BigInt *a,const BigInt *b,int is_signed,BigInt **rem);

BigInt *bi_neg(const BigInt *x);

#endif
