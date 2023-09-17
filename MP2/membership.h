#include <iostream>
#include <chrono>
#include <mutex>
#include <cstring>
#include <vector>
#include <map>
#include <iterator>
#include <algorithm>
using namespace std;

struct MemberEntry {
    int64_t time_stamp_ms;
    int64_t heart_beat_counter;
    int status;// 0: failre, 1: suspect, 2: alive
    int64_t incarnation_count;
    MemberEntry() {
        time_stamp_ms = 0;
        heart_beat_counter = 0;
        status = 2;
        incarnation_count = 0;
    }
    MemberEntry(int64_t current_time_ms ) {
        time_stamp_ms = current_time_ms;
        heart_beat_counter = current_time_ms;
        status = 2;
        incarnation_count = 0;
    }
};

mutex member_status_lock;
map<pair<string,int64_t>, MemberEntry> member_status;
// id: <ip_address, time_stamp>

pair<string, int64_t> machine_id;
 
bool suspection_mode = 0;// only one writter, no mutex needed

inline void print_current_mode(){
    if(suspection_mode) puts("In Gossip+Suspection Mode");
    else puts("In Gossip Mode");
}

void message_receiver();
/*
TODO:
    open socket
    while(true){ 
        (1) receive_message 
            switch(str[0]){
                case 'J':
                    response_join()
                    break;
                case 'G':
                    combine member list
                    break;
                case 'L':
                    update member list
                    break;
                default:
                    throw("FATAL: ")
            }

    }
*/
map<pair<string,int64_t>, MemberEntry> message_to_member_entry(const string &str);

void combine_member_entry(const map<pair<string,int64_t>, MemberEntry> &other);

vector<string> random_choose_send_target(set<string> &previous_sent);

void heartbeat_sender();
/*
TODO:
    while(true){ 
            (1) randomly select, reminder: use lock
            (2) update your own member entry 
            (3) open socket and send the message
            sleep(heartbeat_interval_sec)
        }
*/

string member_entry_to_message();// reminder: use lock

void failure_detector();
/*
TODO:
    while(true){
        check row by row // reminder: use lock
        sleep(heartbeat_interval_sec)
    }
*/

void join_group();
/*
    send message to introducer // message contain ip address & "J"
    while(true){
        if(receive any message){
            update
            break
        }
    }
    wait for any message

*/

void response_join(const string &str);
// deal with ip address

void load_introducer_from_file();

void save_current_status_to_log();


int64_t cur_time_in_ms(){
    return (int64_t) chrono::duration_cast<chrono::milliseconds>(
            chrono::system_clock::now().time_since_epoch()).count();
}