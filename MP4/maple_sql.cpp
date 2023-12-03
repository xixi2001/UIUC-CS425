#include<fstream>
#include<iostream>
#include<string>
#include<sstream>
#include<map>
#include<vector>
#include<regex>
using namespace std;

int main(int argc, char *argv[]){
    ifstream input(argv[1]);
    const auto regex_req = regex (argv[2]);

    string fields_str; getline(input, fields_str); 
    // vector<string> fields = tokenize(fields_str, ',');

    for (string line; getline(input, line); ) {
        if(regex_search(line, regex_req)) {
            cout << "internal " << line << endl;  
        }       
    }
}