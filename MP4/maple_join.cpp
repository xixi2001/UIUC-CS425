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
int main(int argc, char *argv[]){
    //argv1: file_name,
    //argv2: required field 
    ifstream input(argv[1]);
    const string tar = string(argv[2]);

    string fields_str; getline(input, fields_str); 
    vector<string> fields = tokenize(fields_str, ',');

    for (string line; getline(input, line); ) {
        vector<string> words = tokenize(line, ',');
        for(int i=0; i< min(fields.size(),words.size());i++ ) {
            if(fields[i] == tar){
                cout << words[i] << " " <<  string(argv[1]) + "|" + line << endl;
            }
        }
    }

}