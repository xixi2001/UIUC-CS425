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

string to_string(const vector<string> &v){
    string res;
    for(int i=0;i<v.size();i++){
        res += v[i] + (i < v.size() - 1 ? ",":"");
    }
    res += "\n";
    return res;
}

int main(int argc, char *argv[]){
    srand(19260817);

    int file_cnt = stoi(argv[1]);
    int file_size = stoi(argv[2]);

    ifstream input("TSI.csv");
    string fields_str; getline(input, fields_str); 
    vector<string> fields = tokenize(fields_str, ',');
    vector<vector<string> > field_words = vector<vector<string> >(fields.size(), vector<string>({}));

    for (string line; getline(input, line); ) {
        vector<string> words = tokenize(line, ',');
        for(int i=0; i< words.size();i++ ) {
            field_words[i].push_back(words[i]);
        }
        if(words.size() < fields.size()) {
            field_words.back().push_back("");
        }
    }
    
    for(int file_suf = 1; file_suf <= file_cnt; file_suf++) {
        ofstream out("data_" + to_string(file_suf));
        out << to_string(fields);
        for(int cnt=1;cnt<=file_size;cnt++) {
            vector<string> v;
            for(int i=0;i<fields.size();i++){
                int idx = rand() % field_words[i].size();
                v.push_back(field_words[i][idx]);
            }
            out << to_string(v);      
        }
        out.close();
    }

}