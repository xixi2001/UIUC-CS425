#ifndef _MEMBERSHIP_H
#define _MEMBERSHIP_H

#include <iostream>
#include <fstream>
#include <chrono>
#include <mutex>
#include <cstring>
#include <vector>
#include <map>
#include <iterator>
#include <algorithm>
#include <sstream>
#include <set>
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

int64_t cur_time_in_ms();

inline string node_id_to_string(const pair<string,int64_t> &machine_id);

inline void print_to_membership_log(const string &str, bool print_out);
bool print_cmp(pair <pair<string,int64_t>, MemberEntry > A, pair <pair<string,int64_t>, MemberEntry > B);

void print_membership_list();


void print_detailed_list(const map<pair<string,int64_t>, MemberEntry> & other);

void print_current_mode();

void local_mode_change();

void message_receiver();
/*
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

void send_a_udp_message(string ip, string msg);

void heartbeat_sender();
/*
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

vector<string> get_alive_other_ips();

void leave_group();

void group_mode_change();

void load_introducer_from_file();

void save_current_status_to_log();

set<int> get_current_live_membership_set();

int get_index_from_ip_address(const string &str);

string get_ip_address_from_index(int index);

void start_membership_service(string ip);

void init_ip_list();

#endif