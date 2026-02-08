#!/usr/bin/env bash
set -e

echo "iter,impl,latency_ns,throughput" > results/results.csv
echo "iter,offloads,total" > results/offloads.csv

if [ $# -eq 0 ]; then
  set -- 512 1536 2560 3584 4608 5632 6656 7680 8704 9728
fi

# make clean >/dev/null
# make mock_db >/dev/null

for I in "$@"; do
  make EXTRAFLAGS="-DITER=${I}UL" -B >/dev/null

  ./unbased > out.txt 2>&1 & SPID=$!
  sleep 0.5
  ./client >/dev/null 2>&1
  wait "$SPID"
  echo "$I,unbased,$(awk '/Average response time was/{x=$5} END{print x}' out.txt),$(awk '/Average throughput was/{x=$4} END{print x}' out.txt)" >> results/results.csv

  sudo ./based > out.txt 2>&1 & SPID=$!
  sleep 0.5
  ./client >/dev/null 2>&1
  wait "$SPID"
  echo "$I,based,$(awk '/Average response time was/{x=$5} END{print x}' out.txt),$(awk '/Average throughput was/{x=$4} END{print x}' out.txt)" >> results/results.csv
  echo "$I,$(awk '/Offloaded [0-9]+ out of [0-9]+ requests to IO pool/ {print $2 "," $5}' out.txt)"  >> results/offloads.csv

  echo "Done with: $I"
done

rm out.txt
echo "Done: results.csv"
