# Usage of run.sh and plot.py
```sh
make clean
make mock_db
./run.sh $(seq 512 8192 1008128) # Or ./run.sh <ITER1> <ITER2> ...
python3 plot.py results/results.csv # Or python3 plot.py <csv_file>
```
