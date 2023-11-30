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
        for(int i=2; i+1 < argc; i+=2) {
        regex_map[string(argv[i])] = regex (argv[i+1]);
        cout << argv[i] << " "<< argv[i+1] << endl;
    }
    // for(const auto &[field, regex] : regex_map ) {
    //     cout << field << ": " << regex << endl;
    // }

    string fields_str; getline(input, fields_str); 
    vector<string> fields = tokenize(fields_str, ',');

    for (string line; getline(input, line); ) {
        bool ok = 1;
        vector<string> words = tokenize(line, ',');
        for(int i=0; i< min(fields.size(),words.size());i++ ) {
            if(!regex_map.count(fields[i]))continue;
            // cout << "try to match " << words[i] << endl;
            if(!regex_match(words[i], regex_map[fields[i]])) {
                ok = 0;
                break;
            }
        }
        if(ok == 1) {
            cout << "internal " << line << endl;  
        }
    }
}