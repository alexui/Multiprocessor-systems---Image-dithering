#!/bin/bash

make clean
make

let "N=$2"
DIR="./images/"
FILE=$DIR$1
out_openmp="OpenMP.txt"
out_mpi="MPI.txt"
out_threads="Threads.txt"
out_mpi_omp="MPI_OpenMP.txt"
out_mpi_threads="MPI_Threads.txt"

rm $out_openmp
rm $out_mpi
rm $out_threads
rm $out_mpi_omp
rm $out_mpi_threads

for t in 1 2 4 8;
do
	export OMP_NUM_THREADS=$t
	let "OUTPUT=0"
	for i in `seq 1 $N`;
    do
       TIME=`./floydOMP $FILE| awk '{ print $4 }'`
       OUTPUT=`echo $OUTPUT+$TIME | bc`
    done
    OUTPUT=`echo "scale=4; $OUTPUT/$N" | bc -l`
    echo "$t threads : $OUTPUT" >> $out_openmp
done
echo "$out_openmp finished"

for t in 1 2 4 8;
do
	let "OUTPUT=0"
	for i in `seq 1 $N`;
    do
       TIME=`mpirun -n $t floydMPI $FILE| awk '{ print $4 }'`
       OUTPUT=`echo $OUTPUT+$TIME | bc`
    done
    OUTPUT=`echo "scale=4; $OUTPUT/$N" | bc -l`
    echo "$t processes : $OUTPUT" >> $out_mpi
done
echo "$out_mpi finished"
   
for t in 1 2 4 8;
do
	let "OUTPUT=0"
	for i in `seq 1 $N`;
    do
       TIME=`./floydT $t $FILE| awk '{ print $4 }'`
       OUTPUT=`echo $OUTPUT+$TIME | bc`
    done
    OUTPUT=`echo "scale=4; $OUTPUT/$N" | bc -l`
    echo "$t threads : $OUTPUT" >> $out_threads
done
echo "$out_threads finished"

for p in 1 2 4;
do
	echo "$p processes: " >> $out_mpi_omp
	for t in 1 2 4;
	do
		export OMP_NUM_THREADS=$t
		let "OUTPUT=0"
		for i in `seq 1 $N`;
	    do
	       TIME=`mpirun -n $p floydMPIOMP $FILE| awk '{ print $4 }'`
	       OUTPUT=`echo $OUTPUT+$TIME | bc`
	    done
	    OUTPUT=`echo "scale=4; $OUTPUT/$N" | bc -l`
	    echo -e "\t $t threads : $OUTPUT" >> $out_mpi_omp
	done
done
echo "$out_mpi_omp finished"

for p in 1 2 4;
do
	echo "$p processes: " >> $out_mpi_threads
	for t in 1 2 4;
	do
		let "OUTPUT=0"
		for i in `seq 1 $N`;
	    do
	       TIME=`mpirun -n $p floydMPIT $t $FILE| awk '{ print $4 }'`
	       OUTPUT=`echo $OUTPUT+$TIME | bc`
	    done
	    OUTPUT=`echo "scale=4; $OUTPUT/$N" | bc -l`
	    echo -e "\t $t threads : $OUTPUT" >> $out_mpi_threads
	done
done
echo "$out_mpi_threads finished"

cat $out_openmp
echo " "
cat $out_mpi
echo " "
cat $out_threads
echo " "
cat $out_mpi_omp
echo " "
cat $out_mpi_threads