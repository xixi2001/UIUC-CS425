#ifndef _SDFS_H
#define _SDFS_H
#include "membership.h"
#include <filesystem>
#include <set>
using namespace std;


vector<string> tokenize(string input, char delimeter);

vector<string> get_files_from_folder(const string& dir);

void sdfs_message_receiver();
/* TODO:
    message type:
        (1) C: master combine files with the same file name prefix & delete temp files
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

void get_machine_id();

void wait_until_all_files_are_received();

void deleteDirectoryContents(const std::filesystem::path& dir);

#endif