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
        int idx = 0;
        while (ss >> word) {
            /*
                if it is 'yell' command, we cannot split the whole string into words with space.
                
                for example:
                % yell hello,  please     shut up.

                the parsed vector:
                vec[0] = 'yell'
                vec[1] = 'hello,'
                vec[2] = 'please'
                vec[3] = 'shut'
                vec[4] = 'up.'

                all the 'spaces' in message are ignored, it is incorrect.

                ---

                above situation is same in 'tell' command
            */
            vec.push_back(word);
            if(idx == 0 && word == "yell") {
                vec.push_back(command.substr(5));
                break;
            } else if(idx == 1 && vec[0] == "tell") {
                int offset = 4 + 1 + word.length() + 1;
                vec.push_back(command.substr(offset));
                break;
            }
            idx++;
        }
    }

    // store raw command before parsing it
    if(vec.size() >= 1) {
        raw_cmd = vec[0];
        for(int i=1; i<vec.size(); i++) {
            raw_cmd += " " + vec[i];
        }
    } else {
        raw_cmd = "";
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

string UserIO::get_raw_cmd()
{
    return raw_cmd;
}
