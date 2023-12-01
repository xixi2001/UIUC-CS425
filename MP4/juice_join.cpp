#include<fstream>
#include<iostream>
#include<vector>
#include<sstream>
#include<algorithm>
#include<map>
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
    int total = 0;
    map<string,int> cnt;
    string line;
    vector<pair<string,string> > v;
    string key;
    while (getline(cin, line)){
        for(int i=0;i<line.size();i++){
            if(line[i] == ' '){
                key = line.substr(0,i);
                string tmp = line.substr(i+1);
                vector<string> pss = tokenize(tmp, '|');
                v.push_back({pss[0], pss[1]});
                break;
            }
        }
    }
    sort(v.begin(),v.end());
    for(int i=0;i<v.size();i++)
        for(int j=i+1;j<v.size();j++){
        if(v[i].first != v[j].first) {
            cout << key << " " << v[i].second << "," << v[j].second << endl;
        }
    }   
    
    return 0;
}
/*TEST DATA
Radio A|1,2,3,4
Radio B|a,b,c,d
Radio A|2,4,6,8
Radio B|e,f,g,h
*/