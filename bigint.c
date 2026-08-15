#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "bigint.h"

#define BITS_PER_BLK 32
#define DEC_CHUNK 1000000000u
#define DEC_CHUNK_DIGITS 9

/* blk is little endian; bits above width in the top block are always zero */
struct BigInt
{
    uint32_t *blk;
    size_t nblk;
    size_t width;
};

static size_t blks_for(size_t width) {
    return (width+BITS_PER_BLK-1)/BITS_PER_BLK;
}

static void mask_bits(uint32_t *b,size_t n,size_t width) {
    size_t used=width%BITS_PER_BLK;
    if(used&&n)
        b[n-1]&=(uint32_t)((1u<<used)-1u);
}

static size_t bits_in(uint32_t v) {
    size_t b=0;
    while(v)
    {
        v>>=1;
        b++;
    }
    return b;
}

BigInt *bi_zero(size_t width) {
    BigInt *x;
    if(!width||width>SIZE_MAX-BITS_PER_BLK)
        return NULL;
    x=malloc(sizeof(*x));
    if(!x)
        return NULL;
    x->width=width;
    x->nblk=blks_for(width);
    x->blk=calloc(x->nblk,sizeof(uint32_t));
    if(!x->blk)
    {
        free(x);
        return NULL;
    }
    return x;
}

BigInt *bi_copy(const BigInt *x) {
    BigInt *c=bi_zero(x->width);
    if(!c)
        return NULL;
    memcpy(c->blk,x->blk,c->nblk*sizeof(uint32_t));
    return c;
}

void bi_free(BigInt *x) {
    if(!x)
        return;
    free(x->blk);
    free(x);
}

size_t bi_width(const BigInt *x) {
    return x->width;
}

size_t bi_min_bits(const BigInt *x) {
    size_t i=x->nblk;
    while(i)
    {
        if(x->blk[i-1])
            return (i-1)*BITS_PER_BLK+bits_in(x->blk[i-1]);
        i--;
    }
    return 0;
}

int bi_is_zero(const BigInt *x) {
    size_t i;
    for(i=0;i<x->nblk;i++)
        if(x->blk[i])
            return 0;
    return 1;
}

int bi_top_bit(const BigInt *x) {
    size_t pos=x->width-1;
    return (int)((x->blk[pos/BITS_PER_BLK]>>(pos%BITS_PER_BLK))&1u);
}

/* wrap a raw block buffer in a BigInt of minimal width */
static BigInt *from_blocks(const uint32_t *buf,size_t n) {
    BigInt *x;
    size_t bits=0,i=n;
    while(i&&!buf[i-1])
        i--;
    if(i)
        bits=(i-1)*BITS_PER_BLK+bits_in(buf[i-1]);
    x=bi_zero(bits?bits:1);
    if(!x)
        return NULL;
    memcpy(x->blk,buf,x->nblk*sizeof(uint32_t));
    return x;
}

static void mul_add_small(uint32_t *b,size_t n,uint32_t m,uint32_t add) {
    uint64_t carry=add;
    size_t i;
    for(i=0;i<n;i++)
    {
        uint64_t t=(uint64_t)b[i]*m+carry;
        b[i]=(uint32_t)t;
        carry=t>>BITS_PER_BLK;
    }
}

/* 10^len < 16^len, so 4 bits per digit is always enough room */
BigInt *bi_from_dec(const char *s,size_t len) {
    BigInt *x;
    uint32_t *buf;
    size_t n,i=0;
    if(!len||len>SIZE_MAX/4)
        return NULL;
    n=blks_for(len*4);
    buf=calloc(n,sizeof(uint32_t));
    if(!buf)
        return NULL;
    while(i<len)
    {
        uint32_t chunk=0,scale=1;
        size_t k=0;
        while(k<DEC_CHUNK_DIGITS&&i<len)
        {
            chunk=chunk*10u+(uint32_t)(s[i]-'0');
            scale*=10u;
            i++;
            k++;
        }
        mul_add_small(buf,n,scale,chunk);
    }
    x=from_blocks(buf,n);
    free(buf);
    return x;
}

static size_t skip_zeros(const char *s,size_t len) {
    size_t i=0;
    while(i<len&&s[i]=='0')
        i++;
    return i;
}

static uint32_t hex_val(char c) {
    if(c>='0'&&c<='9')
        return (uint32_t)(c-'0');
    if(c>='a'&&c<='f')
        return (uint32_t)(c-'a')+10u;
    return (uint32_t)(c-'A')+10u;
}

BigInt *bi_from_bin(const char *s,size_t len) {
    BigInt *x;
    size_t start=skip_zeros(s,len),nbits=len-start,j;
    if(!nbits)
        return bi_zero(1);
    x=bi_zero(nbits);
    if(!x)
        return NULL;
    for(j=0;j<nbits;j++)
    {
        if(s[start+j]=='1')
        {
            size_t pos=nbits-1-j;
            x->blk[pos/BITS_PER_BLK]|=(uint32_t)1u<<(pos%BITS_PER_BLK);
        }
    }
    return x;
}

/* a 4 bit group never straddles a block, so one or per digit is enough */
BigInt *bi_from_hex(const char *s,size_t len) {
    BigInt *x;
    size_t start=skip_zeros(s,len),ndig=len-start,nbits,k;
    if(!ndig)
        return bi_zero(1);
    if(ndig>(SIZE_MAX-4)/4)
        return NULL;
    nbits=bits_in(hex_val(s[start]))+4*(ndig-1);
    x=bi_zero(nbits);
    if(!x)
        return NULL;
    for(k=0;k<ndig;k++)
    {
        size_t pos=4*k;
        uint32_t v=hex_val(s[len-1-k]);
        x->blk[pos/BITS_PER_BLK]|=v<<(pos%BITS_PER_BLK);
    }
    return x;
}

static int push_grp(uint32_t **g,size_t *cap,size_t *n,uint32_t v) {
    if(*n==*cap)
    {
        size_t ncap=*cap?*cap*2:16;
        uint32_t *p;
        if(*cap>SIZE_MAX/2||ncap>SIZE_MAX/sizeof(uint32_t))
            return 0;
        p=realloc(*g,ncap*sizeof(uint32_t));
        if(!p)
            return 0;
        *g=p;
        *cap=ncap;
    }
    (*g)[(*n)++]=v;
    return 1;
}

/* repeated division by 10^9 gives nine decimal digits per pass */
char *bi_to_dec(const BigInt *x,int is_signed) {
    uint32_t *tmp,*grp=NULL;
    size_t n=x->nblk,gcap=0,ngrp=0,i,d,pos,len;
    int neg=0;
    char *out;

    tmp=malloc(n*sizeof(uint32_t));
    if(!tmp)
        return NULL;
    memcpy(tmp,x->blk,n*sizeof(uint32_t));
    if(is_signed&&bi_top_bit(x))
    {
        uint64_t carry=1;
        neg=1;
        for(i=0;i<n;i++)
            tmp[i]=~tmp[i];
        mask_bits(tmp,n,x->width);
        for(i=0;i<n&&carry;i++)
        {
            uint64_t t=(uint64_t)tmp[i]+carry;
            tmp[i]=(uint32_t)t;
            carry=t>>BITS_PER_BLK;
        }
        mask_bits(tmp,n,x->width);
    }
    do
    {
        uint64_t rem=0;
        size_t j=n;
        while(j)
        {
            uint64_t cur=(rem<<BITS_PER_BLK)|tmp[j-1];
            tmp[j-1]=(uint32_t)(cur/DEC_CHUNK);
            rem=cur%DEC_CHUNK;
            j--;
        }
        if(!push_grp(&grp,&gcap,&ngrp,(uint32_t)rem))
        {
            free(tmp);
            free(grp);
            return NULL;
        }
        while(n&&!tmp[n-1])
            n--;
    } while(n);
    free(tmp);

    d=1;
    for(i=grp[ngrp-1];i>=10;i/=10)
        d++;
    len=d+DEC_CHUNK_DIGITS*(ngrp-1)+(neg?1u:0u);
    out=malloc(len+1);
    if(!out)
    {
        free(grp);
        return NULL;
    }
    out[len]='\0';
    pos=len;
    for(i=0;i<ngrp;i++)
    {
        uint32_t v=grp[i];
        size_t k,cnt=(i+1==ngrp)?d:DEC_CHUNK_DIGITS;
        for(k=0;k<cnt;k++)
        {
            out[--pos]=(char)('0'+v%10u);
            v/=10u;
        }
    }
    if(neg)
        out[--pos]='-';
    free(grp);
    return out;
}
