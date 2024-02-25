#!/bin/bash

# start main
./a.out 1234



# start node 1 
./sensor 112 4 127.0.0.1 1234

# start node 2
./sensor 37 3 127.0.0.1 1234

#start node 3
./sensor 15 2 127.0.0.1 1234

#start node 4
./sensor 129 3.5 127.0.0.1 1234
