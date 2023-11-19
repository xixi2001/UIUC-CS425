#ifndef _MAPLE_JUICE_H
#define _MAPLE_JUICE_H
#include "sdfs.h"
#include <queue>
using namespace std;

void send_mj_message();
/* TODO:
    (1) M: initiate a maple command (send to leader)
    (2) m: send maple command to worker (send to worker)
*/

queue<string> command_queue;
mutex command_queue_lock;
void mj_message_receiver();
/* TODO:
    (1) M: queue commands
    (2) m: initiate work_maple_task 
*/


void command_queue_listener(); // only at leader
/* TODO:
    while (1) {
        get command from queue if not empty
        devide input_file into chunks
        initiate maple_task_monitor for the chunk
    }
    
*/

void work_maple_task(const string& cmd, int socket_num);
/* TODO:
    (1) get files from sdfs
    (2) call maple_exec
    (3) Put execution result
    (4) send success (S message) to leader & delete temp files
*/

void maple_task_monitor(const string& cmd, const vector<string>& files);
/* TODO: 
    try{
        decide sending to which worker
        distribute the command to the worker & wait for S message
        (if socket receive 0 before receive S message => throw error)
    }
    catch{
        initiate another maple_task_monitor()
    }
        
*/


#endif