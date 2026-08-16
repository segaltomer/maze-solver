#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "expression.h"
#include "bigint.h"

#define ST_OK 0
#define ST_INVALID 1
#define ST_SYSERR 2

typedef enum
{
    MODE_SIGNED,
    MODE_UNSIGNED,
    MODE_UNLIMITED
} Mode;

typedef struct
{
    int kind; /* 0 number, 1 '(', 2 ')', 3 operator */
    char op;
    BigInt *val;
} Tok;

static const char *sys_err=NULL;
static Mode mode;
static size_t nbits;

const char *expr_error(void) {
    return sys_err;
}

/* every classifier takes an unsigned char; plain char may be signed */
static int is_ws(unsigned char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r';
}

static int is_dig(unsigned char c) {
    return c>='0'&&c<='9';
}

static int is_bin(unsigned char c) {
    return c=='0'||c=='1';
}

static int is_hex(unsigned char c) {
    return is_dig(c)||(c>='a'&&c<='f')||(c>='A'&&c<='F');
}

/* a directory opens fine on linux but fails on read, so ferror is checked */
static int read_file(const char *path,char **out,size_t *len) {
    FILE *f;
    char *buf=NULL;
    size_t n=0,cap=0;

    *out=NULL;
    *len=0;
    f=fopen(path,"rb");
    if(!f)
    {
        sys_err="cannot open expression file";
        return 0;
    }
    for(;;)
    {
        size_t got;
        if(n==cap)
        {
            size_t ncap=cap?cap*2:4096;
            char *p;
            if(cap>SIZE_MAX/2)
            {
                sys_err="expression file is too large";
                free(buf);
                fclose(f);
                return 0;
            }
            p=realloc(buf,ncap);
            if(!p)
            {
                sys_err="out of memory while reading expression file";
                free(buf);
                fclose(f);
                return 0;
            }
            buf=p;
            cap=ncap;
        }
        got=fread(buf+n,1,cap-n,f);
        n+=got;
        if(got==0)
            break;
    }
    if(ferror(f))
    {
        sys_err="cannot read expression file";
        free(buf);
        fclose(f);
        return 0;
    }
    fclose(f);
    *out=buf;
    *len=n;
    return 1;
}

/* trailing blanks are allowed, leading blanks and any other text are not */
static int only_blanks(const char *s,size_t i,size_t len) {
    while(i<len)
    {
        unsigned char c=(unsigned char)s[i];
        if(c!=' '&&c!='\t'&&c!='\r')
            return 0;
        i++;
    }
    return 1;
}

static int match_word(const char *s,size_t len,const char *w) {
    size_t n=strlen(w);
    return len>=n&&!memcmp(s,w,n);
}

static int parse_width(const char *s,size_t i,size_t len,size_t *out) {
    size_t v=0,d=0;
    while(i<len&&is_dig((unsigned char)s[i]))
    {
        size_t dig=(size_t)(s[i]-'0');
        if(v>(SIZE_MAX-dig)/10)
            return 0;
        v=v*10+dig;
        i++;
        d++;
    }
    if(!d||!v||!only_blanks(s,i,len))
        return 0;
    *out=v;
    return 1;
}

/* leading zeros in the width are accepted; 040 is the decimal integer 40 */
static int parse_header(const char *s,size_t len) {
    size_t i;
    if(match_word(s,len,"UNLIMITED"))
    {
        if(!only_blanks(s,9,len))
            return 0;
        mode=MODE_UNLIMITED;
        nbits=0;
        return 1;
    }
    if(match_word(s,len,"SIGNED"))
    {
        mode=MODE_SIGNED;
        i=6;
    }
    else if(match_word(s,len,"UNSIGNED"))
    {
        mode=MODE_UNSIGNED;
        i=8;
    }
    else
        return 0;
    if(i>=len||(s[i]!=' '&&s[i]!='\t'))
        return 0;
    while(i<len&&(s[i]==' '||s[i]=='\t'))
        i++;
    return parse_width(s,i,len,&nbits);
}

static void free_toks(Tok *t,size_t n) {
    size_t i;
    for(i=0;i<n;i++)
        bi_free(t[i].val);
    free(t);
}

static int push_tok(Tok **t,size_t *cap,size_t *n,Tok v) {
    if(*n==*cap)
    {
        size_t ncap=*cap?*cap*2:64;
        Tok *p;
        if(*cap>SIZE_MAX/2||ncap>SIZE_MAX/sizeof(Tok))
        {
            sys_err="expression is too large";
            return 0;
        }
        p=realloc(*t,ncap*sizeof(Tok));
        if(!p)
        {
            sys_err="out of memory while reading expression";
            return 0;
        }
        *t=p;
        *cap=ncap;
    }
    (*t)[(*n)++]=v;
    return 1;
}

/* a literal keeps its own natural width here; the caller normalizes it */
static int lex_num(const char *s,size_t len,size_t *ip,BigInt **out) {
    size_t i=*ip,st;
    *out=NULL;
    if(s[i]=='0'&&i+1<len&&(s[i+1]=='b'||s[i+1]=='B'))
    {
        st=i+2;
        i=st;
        while(i<len&&is_bin((unsigned char)s[i]))
            i++;
        if(i==st)
            return ST_INVALID;
        *out=bi_from_bin(s+st,i-st);
    }
    else if(s[i]=='0'&&i+1<len&&(s[i+1]=='x'||s[i+1]=='X'))
    {
        st=i+2;
        i=st;
        while(i<len&&is_hex((unsigned char)s[i]))
            i++;
        if(i==st)
            return ST_INVALID;
        *out=bi_from_hex(s+st,i-st);
    }
    else
    {
        st=i;
        while(i<len&&is_dig((unsigned char)s[i]))
            i++;
        *out=bi_from_dec(s+st,i-st);
    }
    if(!*out)
    {
        sys_err="out of memory while reading a literal";
        return ST_SYSERR;
    }
    *ip=i;
    return ST_OK;
}

static int lex(const char *s,size_t len,Tok **out,size_t *nout) {
    Tok *t=NULL;
    size_t cap=0,n=0,i=0;
    int st=ST_OK;

    while(i<len&&st==ST_OK)
    {
        unsigned char c=(unsigned char)s[i];
        Tok tk={0,0,NULL};
        if(is_ws(c))
        {
            i++;
            continue;
        }
        if(is_dig(c))
        {
            st=lex_num(s,len,&i,&tk.val);
            if(st!=ST_OK)
                break;
            tk.kind=0;
        }
        else if(c=='(')
        {
            tk.kind=1;
            i++;
        }
        else if(c==')')
        {
            tk.kind=2;
            i++;
        }
        else if(c=='<'||c=='>')
        {
            if(i+1>=len||s[i+1]!=(char)c)
            {
                st=ST_INVALID;
                break;
            }
            tk.kind=3;
            tk.op=(char)c;
            i+=2;
        }
        else if(c=='~'||c=='*'||c=='/'||c=='%'||c=='+'||c=='-'||c=='&'||c=='^'||c=='|')
        {
            tk.kind=3;
            tk.op=(char)c;
            i++;
        }
        else
        {
            st=ST_INVALID;
            break;
        }
        if(!push_tok(&t,&cap,&n,tk))
        {
            bi_free(tk.val);
            st=ST_SYSERR;
            break;
        }
    }
    if(st!=ST_OK)
    {
        free_toks(t,n);
        return st;
    }
    *out=t;
    *nout=n;
    return ST_OK;
}

static int prec(char op) {
    if(op=='~'||op=='n'||op=='p')
        return 7;
    if(op=='*'||op=='/'||op=='%')
        return 6;
    if(op=='+'||op=='-')
        return 5;
    if(op=='<'||op=='>')
        return 4;
    if(op=='&')
        return 3;
    if(op=='^')
        return 2;
    return 1;
}

static int is_unary(char op) {
    return op=='~'||op=='n'||op=='p';
}

/* fixed modes keep exactly nbits; UNLIMITED uses rule 2 of section 4.3.1 */
static BigInt *norm(BigInt *x) {
    BigInt *r;
    if(!x)
        return NULL;
    r=mode==MODE_UNLIMITED?bi_norm_unl(x):bi_truncate(x,nbits);
    bi_free(x);
    return r;
}

static BigInt *fit(const BigInt *x) {
    return mode==MODE_UNLIMITED?bi_norm_unl(x):bi_truncate(x,nbits);
}

/* 1 when the amount is at least lim, so no huge cast is ever needed */
static int shift_ge(const BigInt *b,size_t lim,size_t *k) {
    if(!bi_to_size(b,k))
        return 1;
    return *k>=lim;
}

/* always consumes a; unary + keeps the value and its width untouched */
static int apply_un(char op,BigInt *a,BigInt **out) {
    BigInt *r;
    *out=NULL;
    if(op=='p')
    {
        *out=a;
        return ST_OK;
    }
    if(op=='n'&&mode==MODE_UNLIMITED)
    {
        if(!bi_is_zero(a))
        {
            bi_free(a);
            return ST_INVALID;
        }
        *out=a;
        return ST_OK;
    }
    r=op=='~'?bi_not(a):bi_neg(a);
    if(mode!=MODE_UNLIMITED)
        r=norm(r);
    bi_free(a);
    if(!r)
    {
        sys_err="out of memory while evaluating";
        return ST_SYSERR;
    }
    *out=r;
    return ST_OK;
}

static int apply_shift(char op,BigInt *a,BigInt *b,BigInt **out) {
    size_t k=0;
    BigInt *r=NULL;
    if(mode==MODE_SIGNED&&bi_top_bit(b))
        return ST_INVALID;
    if(mode==MODE_UNLIMITED)
    {
        if(op=='<')
        {
            if(!bi_to_size(b,&k))
            {
                sys_err="shift amount is too large to allocate";
                return ST_SYSERR;
            }
            r=bi_shl(a,k);
            if(!r)
            {
                sys_err="shift result is too large to allocate";
                return ST_SYSERR;
            }
        }
        else
            r=shift_ge(b,bi_width(a),&k)?bi_zero(bi_width(a)):bi_shr_log(a,k);
    }
    else if(shift_ge(b,nbits,&k))
    {
        if(op=='<'||mode==MODE_UNSIGNED)
            r=bi_zero(nbits);
        else
            r=bi_shr_arith(a,nbits);
    }
    else
        r=norm(op=='<'?bi_shl(a,k):mode==MODE_SIGNED?bi_shr_arith(a,k):bi_shr_log(a,k));
    if(!r)
    {
        sys_err="out of memory while evaluating";
        return ST_SYSERR;
    }
    *out=r;
    return ST_OK;
}

static int apply_arith(char op,BigInt *a,BigInt *b,BigInt **out) {
    BigInt *r=NULL,*rem=NULL;
    if(op=='+')
        r=norm(bi_add(a,b));
    else if(op=='-')
    {
        if(mode==MODE_UNLIMITED)
        {
            if(bi_cmp(a,b)<0)
                return ST_INVALID;
            r=norm(bi_sub_exact(a,b));
        }
        else
            r=bi_sub_wrap(a,b,nbits);
    }
    else if(op=='*')
        r=norm(bi_mul(a,b));
    else
    {
        BigInt *q;
        if(bi_is_zero(b))
            return ST_INVALID;
        q=bi_divmod(a,b,mode==MODE_SIGNED,&rem);
        if(q)
            r=norm(op=='/'?q:rem);
        if(q)
            bi_free(op=='/'?rem:q);
    }
    if(!r)
    {
        sys_err="out of memory while evaluating";
        return ST_SYSERR;
    }
    *out=r;
    return ST_OK;
}

static int apply_bin(char op,BigInt *a,BigInt *b,BigInt **out) {
    BigInt *r=NULL;
    int st=ST_OK;
    *out=NULL;
    if(op=='<'||op=='>')
        st=apply_shift(op,a,b,&r);
    else if(op=='&'||op=='|'||op=='^')
    {
        r=op=='&'?bi_and(a,b):op=='|'?bi_or(a,b):bi_xor(a,b);
        if(mode!=MODE_UNLIMITED)
            r=norm(r);
        if(!r)
        {
            sys_err="out of memory while evaluating";
            st=ST_SYSERR;
        }
    }
    else
        st=apply_arith(op,a,b,&r);
    bi_free(a);
    bi_free(b);
    if(st!=ST_OK)
        return st;
    *out=r;
    return ST_OK;
}

typedef struct
{
    BigInt **v;
    size_t n,cap;
} VStack;

typedef struct
{
    char *o;
    size_t n,cap;
} OStack;

static int vpush(VStack *s,BigInt *x) {
    if(s->n==s->cap)
    {
        size_t ncap=s->cap?s->cap*2:64;
        BigInt **p;
        if(s->cap>SIZE_MAX/2||ncap>SIZE_MAX/sizeof(BigInt*))
            return 0;
        p=realloc(s->v,ncap*sizeof(BigInt*));
        if(!p)
            return 0;
        s->v=p;
        s->cap=ncap;
    }
    s->v[s->n++]=x;
    return 1;
}

static int opush(OStack *s,char c) {
    if(s->n==s->cap)
    {
        size_t ncap=s->cap?s->cap*2:64;
        char *p;
        if(s->cap>SIZE_MAX/2)
            return 0;
        p=realloc(s->o,ncap);
        if(!p)
            return 0;
        s->o=p;
        s->cap=ncap;
    }
    s->o[s->n++]=c;
    return 1;
}

/* pops one operator and applies it, so nesting depth costs heap, not stack */
static int reduce(VStack *vs,OStack *os) {
    char op=os->o[--os->n];
    BigInt *r=NULL;
    int st;
    if(is_unary(op))
    {
        if(vs->n<1)
            return ST_INVALID;
        st=apply_un(op,vs->v[vs->n-1],&r);
        vs->n--;
    }
    else
    {
        if(vs->n<2)
            return ST_INVALID;
        st=apply_bin(op,vs->v[vs->n-2],vs->v[vs->n-1],&r);
        vs->n-=2;
    }
    if(st!=ST_OK)
        return st;
    if(!vpush(vs,r))
    {
        bi_free(r);
        sys_err="out of memory while evaluating";
        return ST_SYSERR;
    }
    return ST_OK;
}

static int eval_toks(Tok *t,size_t n,BigInt **out) {
    VStack vs={NULL,0,0};
    OStack os={NULL,0,0};
    size_t i;
    int st=ST_OK,want=1;

    *out=NULL;
    for(i=0;i<n&&st==ST_OK;i++)
    {
        if(t[i].kind==0)
        {
            BigInt *v;
            if(!want)
            {
                st=ST_INVALID;
                break;
            }
            v=fit(t[i].val);
            if(!v||!vpush(&vs,v))
            {
                bi_free(v);
                sys_err="out of memory while evaluating";
                st=ST_SYSERR;
                break;
            }
            want=0;
        }
        else if(t[i].kind==1)
        {
            if(!want)
                st=ST_INVALID;
            else if(!opush(&os,'('))
            {
                sys_err="out of memory while evaluating";
                st=ST_SYSERR;
            }
        }
        else if(t[i].kind==2)
        {
            if(want)
            {
                st=ST_INVALID;
                break;
            }
            while(os.n&&os.o[os.n-1]!='('&&st==ST_OK)
                st=reduce(&vs,&os);
            if(st==ST_OK&&!os.n)
                st=ST_INVALID;
            else if(st==ST_OK)
                os.n--;
        }
        else
        {
            char op=t[i].op;
            if(want)
            {
                if(op!='~'&&op!='+'&&op!='-')
                {
                    st=ST_INVALID;
                    break;
                }
                op=op=='+'?'p':op=='-'?'n':'~';
                if(!opush(&os,op))
                {
                    sys_err="out of memory while evaluating";
                    st=ST_SYSERR;
                }
            }
            else
            {
                while(os.n&&os.o[os.n-1]!='('&&st==ST_OK
                    &&(prec(os.o[os.n-1])>prec(op)
                    ||(prec(os.o[os.n-1])==prec(op)&&!is_unary(os.o[os.n-1]))))
                    st=reduce(&vs,&os);
                if(st==ST_OK&&!opush(&os,op))
                {
                    sys_err="out of memory while evaluating";
                    st=ST_SYSERR;
                }
                want=1;
            }
        }
    }
    if(st==ST_OK&&want)
        st=ST_INVALID;
    while(st==ST_OK&&os.n)
    {
        if(os.o[os.n-1]=='(')
        {
            st=ST_INVALID;
            break;
        }
        st=reduce(&vs,&os);
    }
    if(st==ST_OK&&vs.n!=1)
        st=ST_INVALID;
    if(st==ST_OK)
        *out=vs.v[0];
    else
    {
        for(i=0;i<vs.n;i++)
            bi_free(vs.v[i]);
    }
    free(vs.v);
    free(os.o);
    return st;
}

ExpressionResult evaluate_expression_file(const char *file_path) {
    ExpressionResult res={EXPR_INVALID,NULL};
    char *buf=NULL;
    size_t len=0,i=0,nt=0;
    Tok *toks=NULL;
    BigInt *val=NULL;
    int st;

    sys_err=NULL;
    if(!read_file(file_path,&buf,&len))
        return res;
    while(i<len&&buf[i]!='\n')
        i++;
    if(!parse_header(buf,i))
    {
        free(buf);
        return res;
    }
    st=i<len?lex(buf+i+1,len-i-1,&toks,&nt):ST_INVALID;
    free(buf);
    if(st==ST_OK)
    {
        st=nt?eval_toks(toks,nt,&val):ST_INVALID;
        free_toks(toks,nt);
    }
    if(st==ST_SYSERR)
        return res;
    if(st!=ST_OK)
    {
        sys_err=NULL;
        return res;
    }
    res.result_str=bi_to_dec(val,mode==MODE_SIGNED);
    bi_free(val);
    if(!res.result_str)
    {
        sys_err="out of memory while formatting the result";
        return res;
    }
    res.status=EXPR_VALID;
    return res;
}
