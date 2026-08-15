#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "maze.h"

typedef struct
{
    char *cells;
    size_t len;
} MazeRow;

typedef struct
{
    MazeRow *rows;
    size_t nrows;
    size_t cap;
} Maze;

static const char *sys_err=NULL;

const char *maze_error(void) {
    return sys_err;
}

static int grow_rows(Maze *m) {
    size_t ncap=m->cap?m->cap*2:8;
    MazeRow *p;
    if(m->cap>SIZE_MAX/2||ncap>SIZE_MAX/sizeof(MazeRow))
    {
        sys_err="maze is too large";
        return 0;
    }
    p=realloc(m->rows,ncap*sizeof(MazeRow));
    if(!p)
    {
        sys_err="out of memory while reading maze";
        return 0;
    }
    m->rows=p;
    m->cap=ncap;
    return 1;
}

static int push_row(Maze *m,const char *buf,size_t len) {
    char *cells;
    if(m->nrows==m->cap&&!grow_rows(m))
        return 0;
    cells=malloc(len+1);
    if(!cells)
    {
        sys_err="out of memory while reading maze";
        return 0;
    }
    if(len)
        memcpy(cells,buf,len);
    cells[len]='\0';
    m->rows[m->nrows].cells=cells;
    m->rows[m->nrows].len=len;
    m->nrows++;
    return 1;
}

static void free_maze(Maze *m) {
    size_t i;
    for(i=0;i<m->nrows;i++)
        free(m->rows[i].cells);
    free(m->rows);
    m->rows=NULL;
    m->nrows=0;
    m->cap=0;
}

/* '\n' ends a row and is not stored; every other byte is kept verbatim */
static int read_maze(const char *path,Maze *m) {
    FILE *f;
    char *buf=NULL;
    size_t len=0,cap=0;
    int c,ok=1;

    m->rows=NULL;
    m->nrows=0;
    m->cap=0;
    f=fopen(path,"rb");
    if(!f)
    {
        sys_err="cannot open maze file";
        return 0;
    }
    while((c=getc(f))!=EOF)
    {
        if(c=='\n')
        {
            if(!push_row(m,buf,len))
            {
                ok=0;
                break;
            }
            len=0;
            continue;
        }
        if(len==cap)
        {
            size_t ncap=cap?cap*2:64;
            char *p;
            if(cap>SIZE_MAX/2)
            {
                sys_err="maze row is too long";
                ok=0;
                break;
            }
            p=realloc(buf,ncap);
            if(!p)
            {
                sys_err="out of memory while reading maze";
                ok=0;
                break;
            }
            buf=p;
            cap=ncap;
        }
        buf[len++]=(char)c;
    }
    if(ok&&ferror(f))
    {
        sys_err="cannot read maze file";
        ok=0;
    }
    if(ok&&len&&!push_row(m,buf,len))
        ok=0;
    free(buf);
    fclose(f);
    if(!ok)
        free_maze(m);
    return ok;
}

/* part A cells are only S, E, '#' and ' '; gate characters belong to part C */
static int validate_maze(const Maze *m,size_t *sr,size_t *sc) {
    size_t r,c,ns=0,ne=0;
    for(r=0;r<m->nrows;r++)
    {
        for(c=0;c<m->rows[r].len;c++)
        {
            char ch=m->rows[r].cells[c];
            if(ch=='S')
            {
                ns++;
                *sr=r;
                *sc=c;
            }
            else if(ch=='E')
                ne++;
            else if(ch!='#'&&ch!=' ')
                return 0;
        }
    }
    return ns==1&&ne>0;
}

typedef struct
{
    size_t r,c;
} Pos;

static const char MOVES[4]={'U','D','L','R'};

/* a column past the end of a row is a wall, as is anything off the grid */
static char cell_at(const Maze *m,size_t r,size_t c) {
    if(r>=m->nrows||c>=m->rows[r].len)
        return ' ';
    return m->rows[r].cells[c];
}

static int open_cell(const Maze *m,size_t r,size_t c) {
    char ch=cell_at(m,r,c);
    return ch=='#'||ch=='S'||ch=='E';
}

static int step_to(size_t r,size_t c,int d,size_t *nr,size_t *nc) {
    *nr=r;
    *nc=c;
    if(d==0)
    {
        if(!r)
            return 0;
        *nr=r-1;
    }
    else if(d==1)
        *nr=r+1;
    else if(d==2)
    {
        if(!c)
            return 0;
        *nc=c-1;
    }
    else
        *nc=c+1;
    return 1;
}

static void back_from(char mv,size_t r,size_t c,size_t *pr,size_t *pc) {
    *pr=r;
    *pc=c;
    if(mv=='U')
        *pr=r+1;
    else if(mv=='D')
        *pr=r-1;
    else if(mv=='L')
        *pc=c+1;
    else
        *pc=c-1;
}

static void free_seen(char **s,size_t n) {
    size_t i;
    for(i=0;i<n;i++)
        free(s[i]);
    free(s);
}

/* one byte per cell: 0 unvisited, 'S' start, else the move that reached it */
static char **alloc_seen(const Maze *m) {
    char **s;
    size_t i;
    s=calloc(m->nrows?m->nrows:1,sizeof(char*));
    if(!s)
        return NULL;
    for(i=0;i<m->nrows;i++)
    {
        if(!m->rows[i].len)
            continue;
        s[i]=calloc(m->rows[i].len,1);
        if(!s[i])
        {
            free_seen(s,i);
            return NULL;
        }
    }
    return s;
}

static int q_push(Pos **q,size_t *cap,size_t *n,size_t r,size_t c) {
    if(*n==*cap)
    {
        size_t ncap=*cap?*cap*2:64;
        Pos *p;
        if(*cap>SIZE_MAX/2||ncap>SIZE_MAX/sizeof(Pos))
        {
            sys_err="maze is too large";
            return 0;
        }
        p=realloc(*q,ncap*sizeof(Pos));
        if(!p)
        {
            sys_err="out of memory while solving maze";
            return 0;
        }
        *q=p;
        *cap=ncap;
    }
    (*q)[*n].r=r;
    (*q)[*n].c=c;
    (*n)++;
    return 1;
}

/* walk the recorded moves back from E to S, then emit them forwards */
static char *build_path(char **seen,size_t er,size_t ec,size_t sr,size_t sc) {
    char *path;
    size_t r,c,n=0,i;

    r=er;
    c=ec;
    while(r!=sr||c!=sc)
    {
        back_from(seen[r][c],r,c,&r,&c);
        n++;
    }
    path=malloc(n+1);
    if(!path)
    {
        sys_err="out of memory while solving maze";
        return NULL;
    }
    path[n]='\0';
    r=er;
    c=ec;
    i=n;
    while(r!=sr||c!=sc)
    {
        path[--i]=seen[r][c];
        back_from(seen[r][c],r,c,&r,&c);
    }
    return path;
}

/* 1 path found, 0 unreachable, -1 system error */
static int solve(const Maze *m,size_t sr,size_t sc,char **out) {
    char **seen;
    Pos *q=NULL;
    size_t qcap=0,qn=0,qh=0;
    int res=0,d;

    *out=NULL;
    seen=alloc_seen(m);
    if(!seen)
    {
        sys_err="out of memory while solving maze";
        return -1;
    }
    seen[sr][sc]='S';
    if(!q_push(&q,&qcap,&qn,sr,sc))
        res=-1;
    while(res==0&&qh<qn)
    {
        size_t r=q[qh].r,c=q[qh].c,nr,nc;
        qh++;
        if(cell_at(m,r,c)=='E')
        {
            *out=build_path(seen,r,c,sr,sc);
            res=*out?1:-1;
            break;
        }
        for(d=0;d<4;d++)
        {
            if(!step_to(r,c,d,&nr,&nc)||!open_cell(m,nr,nc)||seen[nr][nc])
                continue;
            seen[nr][nc]=MOVES[d];
            if(!q_push(&q,&qcap,&qn,nr,nc))
            {
                res=-1;
                break;
            }
        }
    }
    free(q);
    free_seen(seen,m->nrows);
    return res;
}

/* TODO part C: gate evaluation is not implemented yet */
MazeResult check_extended_maze(const char *maze_path,const char *expressions_dir) {
    MazeResult res={MAZE_INVALID,NULL};

    (void)maze_path;
    (void)expressions_dir;
    sys_err=NULL;
    return res;
}

MazeResult check_maze(const char *maze_path) {
    MazeResult res={MAZE_INVALID,NULL};
    Maze m;
    size_t sr,sc;
    int found;

    sys_err=NULL;
    if(!read_maze(maze_path,&m))
        return res;
    if(validate_maze(&m,&sr,&sc))
    {
        found=solve(&m,sr,sc,&res.path);
        if(found>0)
            res.status=MAZE_SOLVABLE;
        else if(!found)
            res.status=MAZE_UNSOLVABLE;
    }
    free_maze(&m);
    return res;
}
