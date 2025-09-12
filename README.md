# CSR-Compressed-Sparse-Row-graph-
This project is a C implementation of a Compressed Sparse Row (CSR) graph representation, inspired by the "https://www.usenix.org/system/files/login/articles/login_winter20_16_kelly.pdf"

It constructs a CSR representation from an edge list, supports directed and undirected graphs, allows edge existence queries using binary search, and provides BFS traversal to compute shortest-path distances.

**Features**

1. Builds CSR arrays (N and F) from an edge list(text file).
2. Supports directed and undirected graphs.
3. Detects invalid vertex IDs and warns about self-edges.
4. Provides adjacency listing per vertex.
5. Implements:

      I. has_edge(from, to) → check if an edge exists.
    
     II. bfs(source, dist[]) → compute distances from a source vertex.
    
    III. Defensive error handling with detailed messages.


**Build Instructions**

-> ~gcc -o c csr.c

-> ~./c 9 9 edgelist.txt (for directed graph)

-> ~./c -u 9 9 edgelist.txt (for undirected graph)

-------------------------------------------------------------------------- 

9 → number of vertices

9 → number of edges (if -u is set, edges will be doubled internally)

edgelist.txt → file containing one edge per line (from -> to)

--------------------------------------------------------------------------

