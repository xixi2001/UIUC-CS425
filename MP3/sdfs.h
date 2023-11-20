#include "membership.h"
#include <set>
using namespace std;

vector<string> tokenize(string input, char delimeter);

void sdfs_message_receiver();
/* TODO:
    message type:
        (1) C: combine all files(master and slave) with the same file name prefix & delete temp files
        (2) F: send master file name to connected socket
*/

void send_a_sdfs_message(const string& str, int target_index);

void send_file(string src, string dst, int target_idx, string cmd, bool is_in_sdfs_folder);

void membership_listener();

set<int> get_new_slave_idx_set(const set<int> &membership_set);

set<int> get_new_master_idx_set(const set<int> &membership_set);

int hash_string(const string &str);

int find_master(const set<int> &membership_set, int hash_value);// give the hash value, find corresponding master

int find_next_live_id(const set<int> &s,int x); 

void handle_crash(int crash_idx, const set<int> &new_membership_set, const set<int>& delta_slaves);

void handle_join(int join_idx, const set<int> &new_membership_set, const set<int> &new_slaves, const set<int> &new_masters);

void print_to_sdfs_log(const string& str, bool flag);

void print_current_files();

void start_sdfs_service();

vector<string> get_files_under_folder(const string &prefix);

void wait_until_all_files_are_processed(const vector<string> &files);