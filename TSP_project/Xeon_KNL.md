# Intel Xeon Phi-KNL hardware counters for SIMD calculations:
----------------------------
- r5310c2 - No. of micro-ops retired
- r5320c2 - scalar_simd for sse,avx,avx2, avx-512 uops retired.expect for loads,divison,sqrt
- r5340c2 - packed-simd for sse,avx,avx2, avx-512 uops(both float and int) except for loads, apcked bytes and word multiples

## Hugepages on KNL node 11:
#### Execute cmd:
 ```~/perf stat -e cycles,instructions,cache-misses,cache-references<Plug>PeepOpenage-faults ~/openmpi-3.0.0/build_icc/bin/mpirun -n 68 --map-by core -x LD_PRELOAD=libhugetlbfs.so -x HUGETLB_MORECORE=yes -x OMPI_MCA_memory_ptmalloc2_disable=1  ./2opt ~/tsplib/pla7397.tsp 2```

#### Compilation cmd:
 ```~/openmpi-3.0.0/build_icc/bin/mpicc -o 2opt src/two_opt.c src/vnn.c src/util.c src/hill_climb_mpich.c  -mkl -axMIC-AVX512 -fimf-precision=high -mtune=knl -xHost -qopt-prefetch=2 -funroll-loops -ffast-math -g -I./lib/ -Wall -Wextra -Ofast -lm -fopenmp```
