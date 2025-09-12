#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <assert.h>

#define ERRSTR strerror(errno)
#define S1(s) #s
#define S2(s) S1(s)
#define COORDS __FILE__ ":" S2(__LINE__) ": "
#define BAIL(...) \
    do { fprintf(stderr, COORDS __VA_ARGS__); \
         exit(EXIT_FAILURE); } while (0)
#define CAL(p, n, s) \
    do { if (NULL == ((p) = (int *)calloc((n), (s)))) \
             BAIL("calloc(%lu, %lu): %s\n", (n), (s), ERRSTR); \
    } while (0)

static_assert(sizeof(intmax_t) > sizeof(int), "int sizes");

static int s2i(const char *s) {
    char *p;
    intmax_t r;
    errno = 0;
    r = strtoimax(s, &p, 10);
    if (0 != errno || '\0' != *p || 0 >= r || INT_MAX <= r)
        BAIL("s2i(\"%s\") -> %" PRIdMAX ", errno => %s\n", s, r, ERRSTR);
    return (int)r;
}

static struct {
    int V, E, *N, *F;
} CSR;

static int is_undirected = 0; //undirected graph handling

static void print_adjacencies(void) {
    printf("per-vertex adjacencies:\n");
    for (int a = 1; a <= CSR.V; a++) {
        int *begin = CSR.F + CSR.N[a], *end = CSR.F + CSR.N[1 + a];
        printf("%d:", a);
        for (int *b = begin; b < end; b++)
            printf(" %d", *b);
        printf("\n");
    }
}

static int icmp(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

//BFS to check if edge (from, to) exists
static int has_edge(int from, int to) {
    if (from < 1 || from > CSR.V || to < 1 || to > CSR.V)
        return 0;
    int *begin = CSR.F + CSR.N[from];
    int *end = CSR.F + CSR.N[from + 1];
    int len = end - begin;
    //BFS on sorted adjacencies
    int left = 0, right = len - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (begin[mid] == to)
            return 1;
        if (begin[mid] < to)
            left = mid + 1;
        else
            right = mid - 1;
    }
    return 0;
}

//BFS to compute distances from source vertex
static void bfs(int source, int *dist) {
    if (source < 1 || source > CSR.V)
        BAIL("Invalid source vertex: %d\n", source);
    for (int i = 1; i <= CSR.V; i++)
        dist[i] = -1; //-1 means unreachable
    dist[source] = 0;
    int *queue = (int *)calloc(CSR.V, sizeof(int));
    if (!queue)
        BAIL("calloc queue failed: %s\n", ERRSTR);
    int front = 0, rear = 0;
    queue[rear++] = source;
    while (front < rear) {
        int u = queue[front++];
        int *begin = CSR.F + CSR.N[u], *end = CSR.F + CSR.N[u + 1];
        for (int *v = begin; v < end; v++) {
            if (dist[*v] == -1) {
                dist[*v] = dist[u] + 1;
                queue[rear++] = *v;
            }
        }
    }
    free(queue);
}

int main(int argc, char *argv[]) {
    int a, b, line = 0, t = 0;
    FILE *fp;
    //Check for undirected flag (-u)
    int arg_offset = 0;
    if (argc >= 2 && strcmp(argv[1], "-u") == 0) {
        is_undirected = 1;
        arg_offset = 1;
    }
    if (argc != 4 + arg_offset)
        BAIL("usage: %s [-u] V E edgelistfile\n", argv[0]);
    CSR.V = s2i(argv[1 + arg_offset]);
    CSR.E = s2i(argv[2 + arg_offset]);
    if (is_undirected)
        CSR.E *= 2; //Double edges for undirected graph
    if (NULL == (fp = fopen(argv[3 + arg_offset], "r")))
        BAIL("fopen(\"%s\"): %s\n", argv[3 + arg_offset], ERRSTR);
    CAL(CSR.N, 2 + (size_t)CSR.V, sizeof *CSR.N);
    CAL(CSR.F, (size_t)CSR.E, sizeof *CSR.F);
    //First pass: count out-degrees and validate
    while (2 == fscanf(fp, "%d %d\n", &a, &b)) {
        line++;
        if (0 >= a || a > CSR.V || 0 >= b || b > CSR.V)
            BAIL("%d: bad vertexID: %d %d\n", line, a, b);
        if (a == b)
            fprintf(stderr, "%d: warning: self edge\n", line);
        CSR.N[a]++;
        if (is_undirected)
            CSR.N[b]++; //Add reverse edge
    }
    if (!feof(fp))
        BAIL("parse error after %d lines: %s\n", line, ERRSTR);
    if (line != (is_undirected ? CSR.E / 2 : CSR.E))
        BAIL("%d input lines != %d edges\n", line, is_undirected ? CSR.E / 2 : CSR.E);
    //Compute cumulative sums for N
    for (a = 1; a <= CSR.V; a++) {
        t += CSR.N[a];
        CSR.N[a] = t;
    }
    CSR.N[a] = t; //N[V+1] = E
    assert(CSR.N[1 + CSR.V] == CSR.E);
    //Second pass: fill F and adjust N positions
    rewind(fp);
    while (2 == fscanf(fp, "%d %d\n", &a, &b)) {
        CSR.F[--CSR.N[a]] = b; //Add directed edge a -> b
        if (is_undirected)
            CSR.F[--CSR.N[b]] = a; //Add reverse edge b -> a
    }
    if (0 != fclose(fp))
        BAIL("fclose(): %s\n", ERRSTR);
    //Sort adjacencies for each vertex
    for (a = 1; a <= CSR.V; a++)
        qsort(CSR.F + CSR.N[a], (size_t)(CSR.N[1 + a] - CSR.N[a]),
              sizeof *CSR.F, icmp);
    //Dump CSR format
    printf("dump CSR format:\n"
           "V = %d E = %d\n"
           "N: ", CSR.V, CSR.E);
    for (a = 0; a <= 1 + CSR.V; a++)
        printf(" %d", CSR.N[a]);
    printf("\n"
           "F: ");
    for (a = 0; a < CSR.E; a++)
        printf(" %d", CSR.F[a]);
    printf("\n");
    print_adjacencies();
    //Example adjacency queries
    printf("\nExample adjacency queries:\n");
    printf("Edge (2,1) exists: %d\n", has_edge(2, 1));
    printf("Edge (1,2) exists: %d\n", has_edge(1, 2));
    //Example BFS from vertex 2
    int *dist = (int *)calloc(CSR.V + 1, sizeof(int));
    if (!dist)
        BAIL("calloc dist failed: %s\n", ERRSTR);
    bfs(2, dist);
    printf("\nBFS distances from vertex 2:\n");
    for (a = 1; a <= CSR.V; a++)
        printf("Vertex %d: %d\n", a, dist[a]);
    free(dist);
    free(CSR.N);
    free(CSR.F);
    return 0;
}