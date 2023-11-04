#include "membership.h"
#include <set>
using namespace std;

void tcp_message_receiver();
/*
while receive a message:
    switch(str[0]){
        case 'P':
            //parse the file from string
            udpate master file
            send file to its slaves
            break;
        case 'p':
            update salve files
            break;
        case 'D':
            delete corresponding master file 
            send delete request to its slaves
            break;
        case 'd':
            delete corresponding slave file
            break;
        case 'G':
            send back corresponding master
            break;
        case '':

    }
*/

void send_a_tcp_message(const string& str, int target_index);

void send_file(string src, string dst, int target_idx, string cmd, bool is_in_sdfs_folder);

void membership_listener();

set<int> get_new_slave_idx_set(const set<int> &membership_set);

set<int> get_new_master_idx_set(const set<int> &membership_set);

int hash_string(const string &str);

int find_master(const set<int> &membership_set, int hash_value);// give the hash value, find corresponding master

int find_next_live_id(const set<int> &s,int x); 

void handle_crash(int crash_idx, const set<int> &new_membership_set, const set<int>& delta_slaves);
/*
if is the NEXT machine of crash_idx:
    change corresponding slaves files to master files
    send new master files to all the slaves
for slave: delta_slaves
    send all master files to the new slave

*/

void handle_join(int join_idx, const set<int> &new_membership_set, const set<int> &new_slaves, const set<int> &new_masters);
/*
if is the NEXT machine of join_idx:
    send corresponding master files to join_idx
    delete those files

if is the master of join_idx:
    send all master files to join_idx

delete all the slave files not in new_masters

*/

void print_to_sdfs_log(const string& str, bool flag);

void print_current_files();