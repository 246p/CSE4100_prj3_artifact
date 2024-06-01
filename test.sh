#!/bin/sh
# 0 : show 1: buy 2: sell 3: rand

## Client 수에 따른 시간당 처리율
rm log.txt
for option in 0 1 2 3
do
    rm "task1_s_$option.txt"
    for clinum in 100 200 300 400 500 600 700 800 900 1000
    do
        for iter in 1 2 3 4 5 6 7 8 9 10
        do
            echo "./task1/multiclient 172.30.10.2 60127 $clinum $option"
            ./task1/multiclient 172.30.10.2 60127 $clinum $option > log.txt
            sleep 1s
            cat log.txt | grep "Time" >> "task1_s_$option.txt"
        done 
    done
done

## Worker Thread수에 따른 처리율 차이 (Task2)
# rm log.txt
# clinum=1000
# for option in 3
# do
#     for iter in 1 2 3 4 5 6 7 8 9 10
#     do
#         echo "./task2/multiclient 172.30.10.2 60127 $clinum $option"
#         ./task2/multiclient 172.30.10.2 60127 $clinum $option > log.txt
#         sleep 1s
#         cat log.txt | grep "Time" >> "task2_worker.txt"
#      done
# done


## Stock ID의 수 에 따른 처리율 차이 (Task2)
# rm log.txt
# clinum=1000
# for option in 2
# do
#     for maxstock in 1 2 3 4 5 6 7 8 9 10
#     do
#         for iter in 1 2 3 4 5 6 7 8 9 10
#         do
#             echo "./task2/multiclient 172.30.10.2 60127 $clinum $option $maxstock"
#             ./task2/multiclient 172.30.10.2 60127 $clinum $option $maxstock > log.txt
#             sleep 1s
#             cat log.txt | grep "Time" >> "task2_max.txt"
#         done 
#     done
# done
