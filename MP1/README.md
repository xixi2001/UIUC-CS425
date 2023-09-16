# UIUC-CS425

## MP1 Distributed Log Querier

### Server

Server should be run on every machine that requires grep service.

To compile and run the server:
```
g++ -std=c++17 -o server server.cpp
./server [machine_index]
```

[machine_index] here refers to the number you assign the machine.

### Client

To compile and run the client:
```
g++ -std=c++17 -pthread -o client client.cpp
./client
```

then type in the grep command and client will grep all the logs from the available servers.

### Unit Test

To generate the test data:
```
g++ -std=c++17 -o data_generator data_generator.cpp
./data_generator [generation_option]
```
[generation_option] can be frequent, not very frequent, rare and all.

Then use `sh test.sh` to run the test.
