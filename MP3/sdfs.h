#include "membership.h"
using namespace std;

set<string> master_files; // files save as master; string is the file name
set<pair<string,int> > slave_files;  
// files save as slave;string is file name; int is hash value

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

vector<int> last_alive_membership_list;

void membership_list_listener();

vector<int> slave_id;

int machine_idx;

vector<int> get_new_slave_id();

vector<int> get_new_master_id();

int hash_string(const string &str);

int find_master();// give the hash value, find corresponding master

void handle_crash(int crash_idx);
/*
if is the NEXT machine of crash_idx:
    change corresponding slaves files to master files
    send new master files to all the slaves
if slave_id != get_new_slave_id()
    send all master files to the new slave

slave_id = get_new_slave_id();
*/

void handle_join(int join_idx);
/*
if is the NEXT machine of join_idx:
    send corresponding master files to join_idx
    delete those files AND send 'd' to slaves

if is the master of join_idx:
    send all master diles to join_idx

delete all the slave files not in get_new_master_id()
*/

void print_to_sdfs_log(const string& str, bool flag);