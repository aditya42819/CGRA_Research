#include "route_kernel.hpp"

static int neighborOf(int u, int d) {
    int row = u / COLS, col = u % COLS;
    if (d == 0 && row > 0)        return u - COLS;
    if (d == 1 && row < ROWS - 1) return u + COLS;
    if (d == 2 && col > 0)        return u - 1;
    if (d == 3 && col < COLS - 1) return u + 1;
    return -1;
}

int route_kernel(int src, unsigned reach_mask, int target, int slot,
                 const int link_owner[NUM_PE][DIRS][MAX_II],
                 int path_out[NUM_PE + 1], int* path_len,
                 int nl_pe[NUM_PE], int nl_dir[NUM_PE], int* nl_len)
{
    *path_len = 0;
    *nl_len = 0;

    if (reach_mask & (1u << target)) {
        path_out[0] = target;
        *path_len = 1;
        return 1;
    }

    unsigned visited = reach_mask;
    int parent_pe[NUM_PE], parent_dir[NUM_PE];
    for (int i = 0; i < NUM_PE; ++i) { parent_pe[i] = -1; parent_dir[i] = -1; }

    int queue[NUM_PE];
    int head = 0, tail = 0;
    for (int p = 0; p < NUM_PE; ++p)
        if (reach_mask & (1u << p)) queue[tail++] = p;

    char found = 0;
    for (int it = 0; it < NUM_PE && !found; ++it) {
        if (head >= tail) break;
        int u = queue[head++];
        for (int d = 0; d < DIRS; ++d) {
            int v = neighborOf(u, d);
            if (v < 0) continue;
            if (visited & (1u << v)) continue;
            int owner = link_owner[u][d][slot];
            if (owner != FREE && owner != src) continue;
            visited |= (1u << v);
            parent_pe[v] = u;
            parent_dir[v] = d;
            queue[tail++] = v;
            if (v == target) { found = 1; break; }
        }
    }
    if (!found) return 0;

    int rpath[NUM_PE + 1], rlink_pe[NUM_PE], rlink_dir[NUM_PE];
    int n = 0, m = 0, cur = target;
    rpath[n++] = cur;
    for (int i = 0; i < NUM_PE; ++i) {
        if (reach_mask & (1u << cur)) break;
        int u = parent_pe[cur], d = parent_dir[cur];
        rlink_pe[m] = u; rlink_dir[m] = d; ++m;
        cur = u;
        rpath[n++] = cur;
    }
    for (int i = 0; i < n; ++i) path_out[i] = rpath[n - 1 - i];
    *path_len = n;

    int k = 0;
    for (int i = m - 1; i >= 0; --i) {
        int u = rlink_pe[i], d = rlink_dir[i];
        if (link_owner[u][d][slot] == FREE) { nl_pe[k] = u; nl_dir[k] = d; ++k; }
    }
    *nl_len = k;
    return 1;
}