#ifndef MAZE_H
#define MAZE_H

typedef enum {
    MAZE_SOLVABLE,
    MAZE_UNSOLVABLE,
    MAZE_INVALID
} MazeStatus;

typedef struct {
    MazeStatus status;
    char *path;
} MazeResult;

MazeResult check_maze(const char *maze_path);
MazeResult check_extended_maze(const char *maze_path, const char *expressions_dir);

#endif
