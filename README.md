# UIUC-CS425

## MP2 Distributed Group Membership

Membership sercive should be run on every machine.

To compile and run the sercive:
```
cd MP2
g++ -std=c++17 -pthread -o membership membership.cpp
./membership [machine_ip] [suspicion_mode]
```

[machine_index] here is the ip address of the current machine.

[suspicion_mode] is 0 or 1 refers weather open the suspicion mechanism.

You can enter `LEAVE` in the command line to leave the group.

You can enter `CHANGE` in the command line to change the suspicion mode while the sercive is running.
