// omp_parallel.cpp
// compile with: /openmp
#include <stdio.h>
#include <omp.h>

int main()
{
    unsigned int nrprocessed = 0;

    #pragma omp parallel num_threads(4) default(none) reduction(+ : nrprocessed)
    {
        while (nrprocessed<5)
        {
            nrprocessed++;
        }
    }
    printf("Count %d\n", nrprocessed);
}