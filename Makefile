all: omp mpi threads mpiomp mpithreads

omp: floyd_steinbergOMP.c
	gcc -fopenmp -Wall floyd_steinbergOMP.c -o floydOMP

mpi: floyd_steinbergMPI.c
	mpicc -Wall floyd_steinbergMPI.c -o floydMPI

threads: floyd_steinbergT.c
	gcc -Wall floyd_steinbergT.c -o floydT -lpthread

mpiomp: floyd_steinbergMPI-OpenMP.c
	mpicc -fopenmp -Wall floyd_steinbergMPI-OpenMP.c -o floydMPIOMP

mpithreads: floyd_steinbergMPIT.c
	mpicc -Wall floyd_steinbergMPIT.c -o floydMPIT -lpthread

clean:
	rm floydOMP floydMPI floydT floydMPIOMP floydMPIT \
	outomp.ppm outmpi.ppm outthreads.ppm outmpiomp.ppm outmpithreads.ppm
