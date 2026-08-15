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

/* placeholder: validation and search are not implemented yet */
MazeResult check_maze(const char *maze_path) {
    MazeResult res={MAZE_INVALID,NULL};
    Maze m;

    sys_err=NULL;
    if(!read_maze(maze_path,&m))
        return res;
    free_maze(&m);
    res.status=MAZE_UNSOLVABLE;
    return res;
}
