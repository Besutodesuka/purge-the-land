#include <iostream>
#include <vector>
#include <string>

using namespace std;

int main(){
    vector<string> msg {"Hello", "world"};

    for (const string &word : msg){
        cout << word << "\n";
    }
    cout << endl;
}