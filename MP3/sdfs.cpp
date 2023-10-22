#include "sdfs.h"
using namespace std;

void tcp_message_receiver(){

}

void send_a_tcp_message(const string& str, int target_index){

}

void membership_list_listener(){

}


vector<int> get_new_slave_id(){

}

vector<int> get_new_master_id(){

}

int hash_string(const string &str){

}

int find_master(){

}

void handle_crash(int crash_idx){

}

void handle_join(int join_idx){

}

void print_to_sdfs_log(const string& str, bool flag){

}

int main(){
    start_membership_service("172.22.94.78");
    cout << get_ip_address_from_index(0) << endl;
}