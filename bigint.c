#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include "bigint.h"

#define BITS_PER_BLK 32
#define DEC_CHUNK 1000000000u
#define DEC_CHUNK_DIGITS 9

/* a ceiling on one allocation request, not a limit on the representation:
   blocks are still sized from the value and grow as needed, but a width this
   far past anything storable cannot finish in usable time or memory. every
   BigInt is born in bi_zero, so refusing here turns an absurd width into the
   system error the spec asks for instead of a hang. 2^27 bits is 16MB per
   value, several thousand times the widest width any example needs */
#define WIDTH_CEIL ((size_t)1<<27)

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
    if(!width||width>WIDTH_CEIL||width>SIZE_MAX-BITS_PER_BLK)
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

/* unsigned comparison; differing widths are handled as zero extension */
int bi_cmp(const BigInt *a,const BigInt *b) {
    size_t i=a->nblk>b->nblk?a->nblk:b->nblk;
    while(i)
    {
        uint32_t av=i-1<a->nblk?a->blk[i-1]:0u;
        uint32_t bv=i-1<b->nblk?b->blk[i-1]:0u;
        if(av!=bv)
            return av>bv?1:-1;
        i--;
    }
    return 0;
}

BigInt *bi_truncate(const BigInt *x,size_t n) {
    BigInt *r=bi_zero(n);
    size_t k;
    if(!r)
        return NULL;
    k=r->nblk<x->nblk?r->nblk:x->nblk;
    memcpy(r->blk,x->blk,k*sizeof(uint32_t));
    mask_bits(r->blk,r->nblk,r->width);
    return r;
}

/* widening to n keeps the extra bits clear, so truncation doubles as the
   zero extension the required-operations list asks for */
BigInt *bi_zext(const BigInt *x,size_t n) {
    return bi_truncate(x,n);
}

/* provided because the required-operations list asks for sign extension, but
   unreachable here: every fixed width value is already stored as exactly N
   bits and UNLIMITED is unsigned throughout, so no operand ever has to be
   widened with its sign. kept so the capability exists and is inspectable */
BigInt *bi_sext(const BigInt *x,size_t n) {
    BigInt *r;
    size_t i;
    if(n<=x->width)
        return bi_truncate(x,n);
    r=bi_zext(x,n);
    if(!r||!bi_top_bit(x))
        return r;
    for(i=x->width;i<n;i++)
        r->blk[i/BITS_PER_BLK]|=(uint32_t)1u<<(i%BITS_PER_BLK);
    return r;
}

/* section 4.3.1 rule 2: minimum bits for the value plus one leading zero */
BigInt *bi_norm_unl(const BigInt *x) {
    size_t mb=bi_min_bits(x);
    if(!mb)
        return bi_zero(1);
    if(mb>SIZE_MAX-1)
        return NULL;
    return bi_truncate(x,mb+1);
}

BigInt *bi_add(const BigInt *a,const BigInt *b) {
    BigInt *r;
    size_t w=a->width>b->width?a->width:b->width,i;
    uint64_t carry=0;
    if(w>SIZE_MAX-1)
        return NULL;
    r=bi_zero(w+1);
    if(!r)
        return NULL;
    for(i=0;i<r->nblk;i++)
    {
        uint64_t t=carry;
        if(i<a->nblk)
            t+=a->blk[i];
        if(i<b->nblk)
            t+=b->blk[i];
        r->blk[i]=(uint32_t)t;
        carry=t>>BITS_PER_BLK;
    }
    mask_bits(r->blk,r->nblk,r->width);
    return r;
}

static BigInt *sub_at(const BigInt *a,const BigInt *b,size_t n) {
    BigInt *r=bi_zero(n);
    size_t i;
    uint64_t borrow=0;
    if(!r)
        return NULL;
    for(i=0;i<r->nblk;i++)
    {
        uint64_t av=i<a->nblk?a->blk[i]:0u;
        uint64_t sub=(i<b->nblk?b->blk[i]:0u)+borrow;
        if(av>=sub)
        {
            r->blk[i]=(uint32_t)(av-sub);
            borrow=0;
        }
        else
        {
            r->blk[i]=(uint32_t)(av+((uint64_t)1<<BITS_PER_BLK)-sub);
            borrow=1;
        }
    }
    mask_bits(r->blk,r->nblk,r->width);
    return r;
}

/* the caller must have checked a >= b; UNLIMITED never wraps */
BigInt *bi_sub_exact(const BigInt *a,const BigInt *b) {
    return sub_at(a,b,a->width>b->width?a->width:b->width);
}

BigInt *bi_sub_wrap(const BigInt *a,const BigInt *b,size_t n) {
    return sub_at(a,b,n);
}

BigInt *bi_mul(const BigInt *a,const BigInt *b) {
    BigInt *r;
    size_t i,j;
    if(a->width>SIZE_MAX-b->width)
        return NULL;
    r=bi_zero(a->width+b->width);
    if(!r)
        return NULL;
    for(i=0;i<a->nblk;i++)
    {
        uint64_t carry=0;
        if(!a->blk[i])
            continue;
        for(j=0;j<b->nblk&&i+j<r->nblk;j++)
        {
            uint64_t t=(uint64_t)a->blk[i]*b->blk[j]+r->blk[i+j]+carry;
            r->blk[i+j]=(uint32_t)t;
            carry=t>>BITS_PER_BLK;
        }
        for(j=i+b->nblk;carry&&j<r->nblk;j++)
        {
            uint64_t t=(uint64_t)r->blk[j]+carry;
            r->blk[j]=(uint32_t)t;
            carry=t>>BITS_PER_BLK;
        }
    }
    mask_bits(r->blk,r->nblk,r->width);
    return r;
}

static int buf_cmp(const uint32_t *a,size_t an,const uint32_t *b,size_t bn) {
    size_t n=an>bn?an:bn;
    while(n)
    {
        uint32_t av=n-1<an?a[n-1]:0u;
        uint32_t bv=n-1<bn?b[n-1]:0u;
        if(av!=bv)
            return av>bv?1:-1;
        n--;
    }
    return 0;
}

static void buf_sub(uint32_t *a,size_t an,const uint32_t *b,size_t bn) {
    uint64_t borrow=0;
    size_t i;
    for(i=0;i<an;i++)
    {
        uint64_t av=a[i];
        uint64_t sub=(i<bn?b[i]:0u)+borrow;
        if(av>=sub)
        {
            a[i]=(uint32_t)(av-sub);
            borrow=0;
        }
        else
        {
            a[i]=(uint32_t)(av+((uint64_t)1<<BITS_PER_BLK)-sub);
            borrow=1;
        }
    }
}

static void buf_shl1(uint32_t *a,size_t n,unsigned bit) {
    unsigned carry=bit;
    size_t i;
    for(i=0;i<n;i++)
    {
        unsigned nc=a[i]>>(BITS_PER_BLK-1);
        a[i]=(a[i]<<1)|carry;
        carry=nc;
    }
}

/* schoolbook binary long division on unsigned magnitudes; a and b may have
   differing widths, zero extended as elsewhere in this file. both outputs
   come back sized to a's width, which is always enough since 0<=q<=a and
   0<=r<=a */
static int udivmod(const BigInt *a,const BigInt *b,BigInt **q_out,BigInt **r_out) {
    size_t rn=(a->nblk>b->nblk?a->nblk:b->nblk)+1,i=a->width;
    uint32_t *q,*r;
    BigInt *qx,*rx;

    q=calloc(a->nblk,sizeof(uint32_t));
    r=calloc(rn,sizeof(uint32_t));
    if(!q||!r)
    {
        free(q);
        free(r);
        return 0;
    }
    while(i)
    {
        unsigned bit;
        i--;
        bit=(unsigned)((a->blk[i/BITS_PER_BLK]>>(i%BITS_PER_BLK))&1u);
        buf_shl1(r,rn,bit);
        if(buf_cmp(r,rn,b->blk,b->nblk)>=0)
        {
            buf_sub(r,rn,b->blk,b->nblk);
            q[i/BITS_PER_BLK]|=(uint32_t)1u<<(i%BITS_PER_BLK);
        }
    }
    qx=bi_zero(a->width);
    rx=qx?bi_zero(a->width):NULL;
    if(!qx||!rx)
    {
        free(q);
        free(r);
        bi_free(qx);
        bi_free(rx);
        return 0;
    }
    memcpy(qx->blk,q,qx->nblk*sizeof(uint32_t));
    memcpy(rx->blk,r,rx->nblk*sizeof(uint32_t));
    mask_bits(qx->blk,qx->nblk,qx->width);
    mask_bits(rx->blk,rx->nblk,rx->width);
    free(q);
    free(r);
    *q_out=qx;
    *r_out=rx;
    return 1;
}

/* division on unsigned magnitudes, sign reapplied afterward and truncated
   back to a's width. this reproduces hardware wraparound (INT_MIN/-1 stays
   INT_MIN) for free, since negating the magnitude and truncating to the
   same width it came from is exactly what two's complement negation does;
   no case ever routes through a built in signed divide */
BigInt *bi_divmod(const BigInt *a,const BigInt *b,int is_signed,BigInt **rem) {
    int nega=is_signed&&bi_top_bit(a);
    int negb=is_signed&&bi_top_bit(b);
    BigInt *am=nega?bi_neg(a):bi_copy(a);
    BigInt *bm=negb?bi_neg(b):bi_copy(b);
    BigInt *q,*r,*qs,*rs;
    if(!am||!bm||!udivmod(am,bm,&q,&r))
    {
        bi_free(am);
        bi_free(bm);
        return NULL;
    }
    bi_free(am);
    bi_free(bm);
    qs=nega!=negb?bi_neg(q):q;
    rs=nega?bi_neg(r):r;
    if(qs!=q)
        bi_free(q);
    if(rs!=r)
        bi_free(r);
    if(!qs||!rs)
    {
        bi_free(qs);
        bi_free(rs);
        return NULL;
    }
    *rem=rs;
    return qs;
}

BigInt *bi_neg(const BigInt *x) {
    BigInt *r=bi_zero(x->width);
    size_t i;
    uint64_t carry=1;
    if(!r)
        return NULL;
    for(i=0;i<r->nblk;i++)
        r->blk[i]=~x->blk[i];
    mask_bits(r->blk,r->nblk,r->width);
    for(i=0;i<r->nblk&&carry;i++)
    {
        uint64_t t=(uint64_t)r->blk[i]+carry;
        r->blk[i]=(uint32_t)t;
        carry=t>>BITS_PER_BLK;
    }
    mask_bits(r->blk,r->nblk,r->width);
    return r;
}

/* the shift amount may be far larger than any built in type */
int bi_to_size(const BigInt *x,size_t *out) {
    uint64_t v=0;
    size_t mb=bi_min_bits(x),i;
    if(mb>sizeof(size_t)*CHAR_BIT||mb>64)
        return 0;
    for(i=x->nblk>2?2:x->nblk;i;i--)
        v=(v<<BITS_PER_BLK)|x->blk[i-1];
    *out=(size_t)v;
    return 1;
}

BigInt *bi_not(const BigInt *x) {
    BigInt *r=bi_zero(x->width);
    size_t i;
    if(!r)
        return NULL;
    for(i=0;i<r->nblk;i++)
        r->blk[i]=~x->blk[i];
    mask_bits(r->blk,r->nblk,r->width);
    return r;
}

/* the shorter operand is zero extended; missing blocks simply read as zero */
static BigInt *bitop(const BigInt *a,const BigInt *b,char op) {
    BigInt *r=bi_zero(a->width>b->width?a->width:b->width);
    size_t i;
    if(!r)
        return NULL;
    for(i=0;i<r->nblk;i++)
    {
        uint32_t av=i<a->nblk?a->blk[i]:0u;
        uint32_t bv=i<b->nblk?b->blk[i]:0u;
        r->blk[i]=op=='&'?(av&bv):op=='|'?(av|bv):(av^bv);
    }
    mask_bits(r->blk,r->nblk,r->width);
    return r;
}

BigInt *bi_and(const BigInt *a,const BigInt *b) {
    return bitop(a,b,'&');
}

BigInt *bi_or(const BigInt *a,const BigInt *b) {
    return bitop(a,b,'|');
}

BigInt *bi_xor(const BigInt *a,const BigInt *b) {
    return bitop(a,b,'^');
}

/* appends k zero bits on the right; no high bit is ever discarded */
BigInt *bi_shl(const BigInt *x,size_t k) {
    BigInt *r;
    size_t lsh=k/BITS_PER_BLK,bsh=k%BITS_PER_BLK,i;
    if(x->width>SIZE_MAX-k)
        return NULL;
    r=bi_zero(x->width+k);
    if(!r)
        return NULL;
    for(i=0;i<x->nblk;i++)
    {
        uint64_t v=(uint64_t)x->blk[i]<<bsh;
        size_t j=i+lsh;
        if(j<r->nblk)
            r->blk[j]|=(uint32_t)v;
        if(bsh&&j+1<r->nblk)
            r->blk[j+1]|=(uint32_t)(v>>BITS_PER_BLK);
    }
    mask_bits(r->blk,r->nblk,r->width);
    return r;
}

/* logical right shift keeps the current width, even when k >= width */
BigInt *bi_shr_log(const BigInt *x,size_t k) {
    BigInt *r=bi_zero(x->width);
    size_t lsh,bsh,i;
    if(!r||k>=x->width)
        return r;
    lsh=k/BITS_PER_BLK;
    bsh=k%BITS_PER_BLK;
    for(i=0;i+lsh<x->nblk;i++)
    {
        uint64_t v=x->blk[i+lsh]>>bsh;
        if(bsh&&i+lsh+1<x->nblk)
            v|=(uint64_t)x->blk[i+lsh+1]<<(BITS_PER_BLK-bsh);
        r->blk[i]=(uint32_t)v;
    }
    mask_bits(r->blk,r->nblk,r->width);
    return r;
}

BigInt *bi_shr_arith(const BigInt *x,size_t k) {
    BigInt *r;
    size_t i;
    int neg=bi_top_bit(x);
    if(k>=x->width)
    {
        r=bi_zero(x->width);
        if(!r||!neg)
            return r;
        for(i=0;i<r->nblk;i++)
            r->blk[i]=0xFFFFFFFFu;
        mask_bits(r->blk,r->nblk,r->width);
        return r;
    }
    r=bi_shr_log(x,k);
    if(!r||!neg)
        return r;
    for(i=x->width-k;i<x->width;i++)
        r->blk[i/BITS_PER_BLK]|=(uint32_t)1u<<(i%BITS_PER_BLK);
    return r;
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
