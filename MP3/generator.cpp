#include <iostream>
#include <fstream>
#include <cstring>
#include <cassert>
#include <ctime>
using namespace std;
string s="1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM";

int main(){
    srand(19260817);
    ofstream fout("large");
    int len = 400 * 1024 * 1024;
    string res;
    for(int i=1;i<=len;i++){
        res += s[rand() % s.length()];
    }
    fout << res;
}