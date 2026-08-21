#pragma once

#define ROWS 4
#define COLS 4
#define NUM_PE 16
#define DIRS 4
#define MAX_II 8
#define FREE (-1)

int route_kernel(
    int src,
    unsigned reach_mask,
    int target,
    int slot,
    const int link_owner[NUM_PE][DIRS][MAX_II],
    int path_out[NUM_PE + 1],
    int* path_len,
    int nl_pe[NUM_PE],
    int nl_dir[NUM_PE],
    int* nl_len);