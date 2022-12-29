#include "userio.h"

UserIO::UserIO(bool init_env)
{
    // change cout to non-buffered
    cout << unitbuf;

    // initialize environment variable, PATH
    if(init_env) {
        setenv("PATH", "bin:.", true);
    }
}

void UserIO::err_sys(string msg) {
    perror(msg.c_str());
    exit(-1);
}

void UserIO::output(string msg)
{
    cout << msg;
    if(cout.bad()) {
        err_sys("cout error");
    }
}

void UserIO::outerr(string msg)
{
    cerr << msg;
    if(cerr.bad()) {
        err_sys("cerr error");
    }
}

vector<string> UserIO::input()
{
    string command;
    vector<string> vec;
    getline(cin, command);

    if(cin.bad()) {
        err_sys("cin error");
    } else if(cin.eof()) {
        exit(0);
    } else {
        istringstream ss(command);
        string word;
        while (ss >> word) {
            vec.push_back(word);
        }
    }

    return vec;
}

vector<string> UserIO::string_split(string s, string delimiter) {
    vector<string> result;
    size_t pos = 0;
    string token;

    while ((pos = s.find(delimiter)) != string::npos) {
            token = s.substr(0, pos);
            result.push_back(token);
            s.erase(0, pos + delimiter.length());
    }

    result.push_back(s);
    return result;
}
