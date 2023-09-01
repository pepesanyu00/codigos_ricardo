#!/bin/bash

exec=("TM")

inputs=("-n 10000 -c 1"
        "-n 10000 -c 5"
        "-n 10000 -c 10"
        "-n 10000 -c 15"
        "-n 20000 -c 1"
        "-n 20000 -c 5"
        "-n 20000 -c 10"
        "-n 20000 -c 15")

threads="1"

for j in {1..1}; do
  echo "######################################################"
  echo "###################### Loop $j ######################"
  echo "######################################################"
  for i in "${inputs[@]}"; do
    for th in $threads; do
      for e in "${exec[@]}"; do
        echo "## $j ##./recurrence_$e $i -t $th"
        ./recurrence_$e $i -t $th
      done;
    done;
  done;
done;

