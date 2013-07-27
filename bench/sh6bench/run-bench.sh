#!/bin/bash

export LD_LIBRARY_PATH=../../dist/

LOOPRUN=10
ALLOCATOR=( MALLOC MALLOC_LEAK STR STR_LEAK STM STM_LEAK STRMC STRMC_LEAK )

#CALLCOUNT=( 250 500 750 1000 5000 10000 )
CALLCOUNT=( 10 20 50 )

CASES=( singlethreaded multithreaded )
#CASES=( multithreaded )

mkdir -p bench_results;

for cas in ${CASES[@]}
do
	if [ $cas = "multithreaded" ]; then
		o="-DSYS_MULTI_THREAD -pthread"
	fi
	echo $o

	for c in ${CALLCOUNT[@]}
	do

		echo "";
		echo "";
		echo "-------------------- Throughput --------------------";
		echo "";
		echo "";

		cd ../../; make clean; make > bench/sh6bench/buildlog.txt ; cd -;
		if test -f ../../dist/libscm.so; then

			make clean
			echo "make BENCH_OPTION=\"-DCALL_COUNT='$c' -DPRINTTHROUGHPUT $o\""
			make BENCH_OPTION="-DCALL_COUNT='$c' -DPRINTTHROUGHPUT $o" > buildlog.txt
			for a in ${ALLOCATOR[@]}
			do
				if test -f dist/sh6bench${a}; then
					echo "Started measurement of $a throughput performance. sh6bench call count is $c";
					echo "" > bench_results/throughput_${cas}_${a}_${c}.dat;
					sum=0
					for ((i=1;i<=$LOOPRUN;i++))
					do
						cputime=$(./dist/sh6bench$a | sed -n 's/throughput //p');
						sum=$(( $sum + $cputime ));
						#echo "cputime: $cputime"
						#echo "sum: $sum"
						echo "$cputime" >> bench_results/throughput_${cas}_${a}_${c}.dat;
					done
					avg=$(( $sum / $LOOPRUN ));
					sd=
					echo "AVG total execution time: $avg";
					echo "---" >> bench_results/throughput_${cas}_${a}_${c}.dat;
					echo "$avg" >> bench_results/throughput_${cas}_${a}_${c}.dat;
					mail -s "Benchmark sh6bench${a} [throughput, ${cas}, ${c}] finished" steffueue@gmail.com <bench_results/throughput_${cas}_${a}_${c}.dat;
					#scp bench_results/throughput_${cas}_${a}_${c}.dat stexx@80.218.210.143:~/bench_results_new/throughput/${cas} 
				else
					echo "Build of sh6bench${a} (throughput) failed";
					mail -s "Build of sh6bench${a} (throughput) failed" steffueue@gmail.com <buildlog.txt
					exit
				fi
			done
		else
			echo "Build of libscm.so failed";
			mail -s "Build of libscm.so failed" steffueue@gmail.com <buildlog.txt
			exit
		fi
	done

	
	for c in 250
	do
		cd ../../; make clean; make > bench/sh6bench/buildlog.txt ; cd -;
		
		if test -f ../../dist/libscm.so; then
			echo "";
			echo "";
			echo "-------------------- Latency --------------------";
			echo "";
			echo "";

			make clean
			echo "make BENCH_OPTION=\"-DCALL_COUNT='$c' -DPRINTLATENCY $o\""
			make BENCH_OPTION="-DCALL_COUNT='$c' -DPRINTLATENCY $o" > buildlog.txt
			for a in ${ALLOCATOR[@]}
			do
				if test -f dist/sh6bench${a}; then
					echo "Started measurement of $a latency. sh6bench call count is $c";
					dist/sh6bench$a | sed -n 's/latency //p' >bench_results/latency_${cas}_${a}_${c}.dat;
					tail -n 5 bench_results/latency_${cas}_${a}_${c}.dat | mail -s "Benchmarks sh6bench${a} [latency, ${cas}, ${c}] finished" steffueue@gmail.com;
				else
					mail -s "Build of sh6bench${a} (printlatency) failed" steffueue@gmail.com <buildlog.txt
					exit
				fi
			done
			sed -n 's/scm_create_region //p' <bench_results/latency_${cas}_STR_${c}.dat | gzip >bench_results/latency_scm_create_region_${cas}_STR_${c}.gz
			#scp bench_results/latency_scm_create_region_${cas}_STR_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_create_region_${cas}_STR_${c}.gz
			sed -n 's/scm_create_region //p' <bench_results/latency_${cas}_STRMC_${c}.dat | gzip >bench_results/latency_scm_create_region_${cas}_STRMC_${c}.gz
			#scp bench_results/latency_scm_create_region_${cas}_STRMC_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_create_region_${cas}_STRMC_${c}.gz
			sed -n 's/scm_refresh_region //p' <bench_results/latency_${cas}_STR_${c}.dat | gzip >bench_results/latency_scm_refresh_region_${cas}_STR_${c}.gz
			#scp bench_results/latency_scm_refresh_region_${cas}_STR_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_refresh_region_${cas}_STR_${c}.gz
			sed -n 's/scm_refresh_region_with_clock //p' <bench_results/latency_${cas}_STRMC_${c}.dat | gzip >bench_results/latency_scm_refresh_region_with_clock_${cas}_STRMC_${c}.gz
			#scp bench_results/latency_scm_refresh_region_with_clock_${cas}_STRMC_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_refresh_region_with_clock_${cas}_STRMC_${c}.gz
			sed -n 's/scm_refresh //p' <bench_results/latency_${cas}_STM_${c}.dat | gzip >bench_results/latency_scm_refresh_${cas}_STM_${c}.gz
			#scp bench_results/latency_scm_refresh_${cas}_STM_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_refresh_${cas}_STM_${c}.gz
			sed -n 's/scm_malloc //p' <bench_results/latency_${cas}_STM_${c}.dat | gzip >bench_results/latency_scm_malloc_${cas}_STM_${c}.gz
			#scp bench_results/latency_scm_malloc_${cas}_STM_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_malloc_${cas}_STM_${c}.gz
			sed -n 's/scm_malloc_in_region //p' <bench_results/latency_${cas}_STR_${c}.dat | gzip >bench_results/latency_scm_malloc_in_region_${cas}_STR_${c}.gz
			#scp bench_results/latency_scm_malloc_in_region_${cas}_STR_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_malloc_in_region_${cas}_STR_${c}.gz
			sed -n 's/scm_malloc_in_region //p' <bench_results/latency_${cas}_STRMC_${c}.dat | gzip >bench_results/latency_scm_malloc_in_region_${cas}_STRMC_${c}.gz
			#scp bench_results/latency_scm_malloc_in_region_${cas}_STRMC_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_malloc_in_region_${cas}_STRMC_${c}.gz
			sed -n 's/scm_tick //p' <bench_results/latency_${cas}_STR_${c}.dat | gzip >bench_results/latency_scm_tick_${cas}_STR_${c}.gz
			#scp bench_results/latency_scm_tick_${cas}_STR_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_tick_${cas}_STR_${c}.gz
			sed -n 's/scm_tick //p' <bench_results/latency_${cas}_STM_${c}.dat | gzip >bench_results/latency_scm_tick_${cas}_STM_${c}.gz
			#scp bench_results/latency_scm_tick_${cas}_STM_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_tick_${cas}_STM_${c}.gz
			sed -n 's/scm_tick_clock //p' <bench_results/latency_${cas}_STRMC_${c}.dat | gzip >bench_results/latency_scm_tick_clock_${cas}_STRMC_${c}.gz
			#scp bench_results/latency_scm_tick_clock_${cas}_STRMC_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_tick_clock_${cas}_STRMC_${c}.gz
			sed -n 's/scm_unregister_region //p' <bench_results/latency_${cas}_STR_${c}.dat | gzip >bench_results/latency_scm_unregister_region_${cas}_STR_${c}.gz
			#scp bench_results/latency_scm_unregister_region_${cas}_STR_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_unregister_region_${cas}_STR_${c}.gz
			sed -n 's/scm_unregister_region //p' <bench_results/latency_${cas}_STRMC_${c}.dat | gzip >bench_results/latency_scm_unregister_region_${cas}_STRMC_${c}.gz
			#scp bench_results/latency_scm_unregister_region_${cas}_STRMC_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_unregister_region_${cas}_STRMC_${c}.gz
			
			sed -n 's/malloc //p' <bench_results/latency_${cas}_MALLOC_${c}.dat | gzip >bench_results/latency_malloc_${cas}_MALLOC_${c}.gz
			#scp bench_results/latency_malloc_${cas}_MALLOC_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_malloc_${cas}_MALLOC_${c}.gz
			sed -n 's/free //p' <bench_results/latency_${cas}_MALLOC_${c}.dat | gzip >bench_results/latency_free_${cas}_MALLOC_${c}.gz
			#scp bench_results/latency_free_${cas}_MALLOC_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_free_${cas}_MALLOC_${c}.gz


			sed -n 's/scm_create_region //p' <bench_results/latency_${cas}_STR_LEAK_${c}.dat | gzip >bench_results/latency_scm_create_region_${cas}_STR_LEAK_${c}.gz
			#scp bench_results/latency_scm_create_region_${cas}_STR_LEAK_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_create_region_${cas}_STR_LEAK_${c}.gz
			sed -n 's/scm_create_region //p' <bench_results/latency_${cas}_STRMC_LEAK_${c}.dat | gzip >bench_results/latency_scm_create_region_${cas}_STRMC_LEAK_${c}.gz
			#scp bench_results/latency_scm_create_region_${cas}_STRMC_LEAK_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_create_region_${cas}_STRMC_LEAK_${c}.gz
			sed -n 's/scm_refresh_region //p' <bench_results/latency_${cas}_STR_LEAK_${c}.dat | gzip >bench_results/latency_scm_refresh_region_${cas}_STR_LEAK_${c}.gz
			#scp bench_results/latency_scm_refresh_region_${cas}_STR_LEAK_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_refresh_region_${cas}_STR_LEAK_${c}.gz
			sed -n 's/scm_refresh_region_with_clock //p' <bench_results/latency_${cas}_STRMC_LEAK_${c}.dat | gzip >bench_results/latency_scm_refresh_region_with_clock_${cas}_STRMC_LEAK_${c}.gz
			#scp bench_results/latency_scm_refresh_region_with_clock_${cas}_STRMC_LEAK_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_refresh_region_with_clock_${cas}_STRMC_LEAK_${c}.gz
			sed -n 's/scm_refresh //p' <bench_results/latency_${cas}_STM_LEAK_${c}.dat | gzip >bench_results/latency_scm_refresh_${cas}_STM_LEAK_${c}.gz
			#scp bench_results/latency_scm_refresh_${cas}_STM_LEAK_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_refresh_${cas}_STM_LEAK_${c}.gz
			sed -n 's/scm_malloc //p' <bench_results/latency_${cas}_STM_LEAK_${c}.dat | gzip >bench_results/latency_scm_malloc_${cas}_STM_LEAK_${c}.gz
			#scp bench_results/latency_scm_malloc_${cas}_STM_LEAK_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_malloc_${cas}_STM_LEAK_${c}.gz
			sed -n 's/scm_malloc_in_region //p' <bench_results/latency_${cas}_STR_LEAK_${c}.dat | gzip >bench_results/latency_scm_malloc_in_region_${cas}_STR_LEAK_${c}.gz
			#scp bench_results/latency_scm_malloc_in_region_${cas}_STR_LEAK_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_malloc_in_region_${cas}_STR_LEAK_${c}.gz
			sed -n 's/scm_malloc_in_region //p' <bench_results/latency_${cas}_STRMC_LEAK_${c}.dat | gzip >bench_results/latency_scm_malloc_in_region_${cas}_STRMC_LEAK_${c}.gz
			#scp bench_results/latency_scm_malloc_in_region_${cas}_STRMC_LEAK_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_malloc_in_region_${cas}_STRMC_LEAK_${c}.gz
			sed -n 's/scm_tick //p' <bench_results/latency_${cas}_STR_LEAK_${c}.dat | gzip >bench_results/latency_scm_tick_${cas}_STR_LEAK_${c}.gz
			#scp bench_results/latency_scm_tick_${cas}_STR_LEAK_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_tick_${cas}_STR_LEAK_${c}.gz
			sed -n 's/scm_tick //p' <bench_results/latency_${cas}_STM_LEAK_${c}.dat | gzip >bench_results/latency_scm_tick_${cas}_STM_LEAK_${c}.gz
			#scp bench_results/latency_scm_tick_${cas}_STM_LEAK_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_tick_${cas}_STM_LEAK_${c}.gz
			sed -n 's/scm_tick_clock //p' <bench_results/latency_${cas}_STRMC_LEAK_${c}.dat | gzip >bench_results/latency_scm_tick_clock_${cas}_STRMC_LEAK_${c}.gz
			#scp bench_results/latency_scm_tick_clock_${cas}_STRMC_LEAK_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_tick_clock_${cas}_STRMC_LEAK_${c}.gz
			sed -n 's/scm_unregister_region //p' <bench_results/latency_${cas}_STR_LEAK_${c}.dat | gzip >bench_results/latency_scm_unregister_region_${cas}_STR_LEAK_${c}.gz
			#scp bench_results/latency_scm_unregister_region_${cas}_STR_LEAK_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_unregister_region_${cas}_STR_LEAK_${c}.gz
			sed -n 's/scm_unregister_region //p' <bench_results/latency_${cas}_STRMC_LEAK_${c}.dat | gzip >bench_results/latency_scm_unregister_region_${cas}_STRMC_LEAK_${c}.gz
			#scp bench_results/latency_scm_unregister_region_${cas}_STRMC_LEAK_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_scm_unregister_region_${cas}_STRMC_LEAK_${c}.gz
			
			sed -n 's/malloc //p' <bench_results/latency_${cas}_MALLOC_LEAK_${c}.dat | gzip >bench_results/latency_malloc_${cas}_MALLOC_LEAK_${c}.gz
			#scp bench_results/latency_malloc_${cas}_MALLOC_LEAK_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_malloc_${cas}_MALLOC_LEAK_${c}.gz
			sed -n 's/free //p' <bench_results/latency_${cas}_MALLOC_LEAK_${c}.dat | gzip >bench_results/latency_free_${cas}_MALLOC_LEAK_${c}.gz
			#scp bench_results/latency_free_${cas}_MALLOC_LEAK_${c}.gz stexx@80.218.210.143:~/bench_results_new/latency/${cas} && rm bench_results/latency_free_${cas}_MALLOC_LEAK_${c}.gz




		else
			echo "Build of libscm.so failed";
			mail -s "Build of libscm.so failed" steffueue@gmail.com <buildlog.txt
			exit
		fi
		echo "";
		echo "";
		echo "-------------------- Memory --------------------";
		echo "";
		echo "";

		cd ../../; make clean; make > bench/sh6bench/buildlog.txt; cd -;
		if test -f ../../dist/libscm.so; then

			make clean
			echo "make BENCH_OPTION=\"-DCALL_COUNT='$c' -DPRINTMEMINFO $o\""
			make BENCH_OPTION="-DCALL_COUNT='$c' -DPRINTMEMINFO $o" > buildlog.txt
			for a in ${ALLOCATOR[@]}
			do
				if test -f dist/sh6bench${a}; then
					echo "Started memory measurement for $a. sh6bench call count is $c";
				dist/sh6bench$a 2>&1 | gzip >bench_results/memory_${cas}_${a}_${c}.gz;
				gzip -dc bench_results/memory_${cas}_${a}_${c}.gz | tail -n 20 | mail -s "Benchmarks sh6bench${a} [memory, ${cas}, ${c}] finished" steffueue@gmail.com;
				#scp bench_results/memory_${cas}_${a}_${c}.gz stexx@80.218.210.143:~/bench_results_new/memory/${cas} && rm bench_results/memory_${cas}_${a}_${c}.gz
			else
				mail -s "Build of sh6bench${a} (printmeminfo) failed" steffueue@gmail.com <buildlog.txt
				exit
			fi
			done
		else
			mail -s "Build of libscm.so failed" steffueue@gmail.com <buildlog.txt
			exit
		fi
	done
done
