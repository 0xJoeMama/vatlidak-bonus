# for your markos
I want you to run this while variating the [ITER parameter](./config.h).
To do that you need to recompile the project as follows.
- Run make mock\_db
- Then run make EXTRAFLAGS=-DITER=\<nr-iterations\>UL

I want you to parse the output of based and unbased, after running them with different ITER parameters.
Then I want you to plot the result on 2 graphs:
* A latency = f(ITER) graph
* A throughput = f(ITER) graph

Then commit those graphs as well as the python script you used here.
Again this whole thing can be automated, don't do it by hand.
Do note that this will take some time to run.
