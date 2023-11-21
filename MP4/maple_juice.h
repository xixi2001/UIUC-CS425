
#ifndef _MJ_H
#define _MJ_H
#include "sdfs.h"
#include <queue>
using namespace std;

void send_mj_message();
/*  (1) M: initiate a maple command (send to leader)
    (2) m: send maple command to worker (send to worker)
    (3) J: initiate a juice command (send to leader)
    (4) j: send juice command to worker (send to worker)
*/

queue<string> command_queue;
mutex command_queue_lock;
void mj_message_receiver();
/*  
    (1) M: queue commands
    (2) m: initiate work_maple_task 
    (3) J: queue commands
    (4) j: initiate juice_maple_task 
*/


void command_queue_listener(); // only at leader
/* TODO:
    while (1) {
        get command from queue if not empty
        divide input_file into chunks
        initiate maple_task_monitor/juice_task_monitor for the chunk
    }
    
*/

void work_maple_task(const string& cmd, int socket_num);
/*
    (1) get files from sdfs
    (2) call maple_exec
    (3) Put execution result
    (4) send success (S message) to leader & delete temp files
*/

void work_juice_task(const string& cmd, int socket_num);
/* TODO:
    (1) get files from sdfs
    (2) call juice_exec
    (3) Put execution result
    (4) send success (S message) to leader & delete temp files
*/


void task_monitor(const string& cmd, const vector<string>& files);
/*
    try{
        decide sending to which worker
        distribute the command to the worker & wait for S message
        (if socket receive 0 before receive S message => throw error)
    }
    catch{
        initiate another task_monitor()
    }
        
*/


void print_to_mj_log(const string& str, bool flag);

#endif