#include<fstream>
#include<iostream>
#include<string>
#include<sstream>
#include<map>
#include<vector>
#include<regex>
using namespace std;

vector<string> tokenize(string input, char delimeter){
    istringstream ss(input);
    string token;
    vector<string> ret;
    while(getline(ss, token, delimeter)){
        ret.push_back(token);
    }
    return ret;
}
map<string, regex> regex_map;

int main(int argc, char *argv[]){
    ifstream input(argv[1]);
    string tar = string(argv[2]);

    string fields_str; getline(input, fields_str); 
    vector<string> fields = tokenize(fields_str, ',');
    const int target_index = 'K' - 'A';

    for (string line; getline(input, line); ) {
        bool ok = 1;
        vector<string> words = tokenize(line, ',');
        if(words[target_index] == tar) {
            cout << tar << " " << words[target_index - 1] << endl;
        }
    }
}