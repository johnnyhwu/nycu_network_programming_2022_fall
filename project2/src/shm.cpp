#include "shm.h"

ShM::ShM()
{
    shm_key = 7777;
    shm_id = -1;
    shm_struct_ptr = NULL;
}

void ShM::create(bool new_one)
{
    if(new_one) {
        shm_id = shmget(shm_key, sizeof(shm_struct), 0666|IPC_CREAT);
    } else {
        shm_id = shmget(shm_key, sizeof(shm_struct), 0);
    }
    
    if (shm_id < 0) {
        cout << "Error: cannot create shared memory !" << "\n";
        return;
    }

    shm_struct_ptr = (shm_struct *) shmat(shm_id, NULL, 0);
}

void ShM::detach()
{
    shmdt(shm_struct_ptr);
}

void ShM::remove()
{
    shmctl(shm_id, IPC_RMID, NULL);
}

void ShM::set_id(int socket_fd_fake)
{
    client_pid = socket_fd_fake;

    for(int i=1; i<100; i++) {
        if(shm_struct_ptr->id[i] == 0) {
            client_id = i;
            shm_struct_ptr->id[i] = socket_fd_fake;
            break;
        }
    }
}

void ShM::set_ip(string ip)
{
    strcpy(shm_struct_ptr->ip[client_id], ip.c_str());
}

void ShM::set_port(int port)
{
    shm_struct_ptr->port[client_id] = port;
}

void ShM::set_name(string name)
{
    strcpy(shm_struct_ptr->name[client_id], name.c_str());
}

void ShM::set_env(string key, string value)
{
    // parse original environment variable string
    vector< vector<string> > original_env;
    stringstream ss1(shm_struct_ptr->env[client_id]);
    string word;
    while (ss1 >> word) {
        stringstream ss2(word);
        string segment;
        vector<string> vec;
        while(getline(ss2, segment, '=')) {
            vec.push_back(segment);
        }
        original_env.push_back(vec);
    }

    // if key is already existing, overwrite its value
    bool is_insert = false;
    for(int i=0; i<original_env.size(); i++) {
        if(original_env[i][0] == key) {
            original_env[i][1] = value;
            is_insert = true;
            break;
        }
    }

    // else add new key and value
    if(is_insert == false) {
        vector<string> new_vec;
        new_vec.push_back(key);
        new_vec.push_back(value);
        original_env.push_back(new_vec);
    }

    // parse original_vec into a single string
    string new_env = original_env[0][0] + "=" + original_env[0][1];
    for(int i=1; i<original_env.size(); i++) {
        new_env += " " + original_env[i][0] + "=" + original_env[i][1];
    }

    // store new_env into shared memory
    strcpy(shm_struct_ptr->env[client_id], new_env.c_str());
}

void ShM::set_message(string msg)
{
    strcpy(shm_struct_ptr->message, msg.c_str());
}

int ShM::set_user_pipe(int src_id, int trg_id, string purpose)
{
    int fd;

    if(purpose == "write") {
        // create fifo
        string fifo = "user_pipe/" + to_string(src_id) + "_" + to_string(trg_id);
        filesystem::path program(fifo.c_str());
        if(filesystem::exists(program)) {
            unlink(fifo.c_str());
        }

        if(mkfifo(fifo.c_str(), 0666) == -1) {
            cout << "Error: cannot create fifo\n";
            return -1;
        }

        // save fifo info in shared memory
        for(int i=0; i<500; i++) {
            if(shm_struct_ptr->user_pipe[i][0] == 0 && shm_struct_ptr->user_pipe[i][1] == 0) {
                shm_struct_ptr->user_pipe[i][0] = src_id;
                shm_struct_ptr->user_pipe[i][1] = trg_id;
                shm_struct_ptr->user_pipe_idx = i;
                break;
            }
        }

        // send signal to another process
        // another process should open same fifo and store info in shared memory
        kill(shm_struct_ptr->id[trg_id], SIGUSR2);

        // open fifo
        fd = open(fifo.c_str(), O_WRONLY);

        shm_struct_ptr->user_pipe[shm_struct_ptr->user_pipe_idx][2] = fd;
    } else if(purpose == "read") {
        int idx = shm_struct_ptr->user_pipe_idx;
        int src_id = shm_struct_ptr->user_pipe[idx][0];
        int trg_id = shm_struct_ptr->user_pipe[idx][1];
        string fifo = "user_pipe/" + to_string(src_id) + "_" + to_string(trg_id);

        // open fifo
        fd = open(fifo.c_str(), O_RDONLY);

        // save fifo info in shared memory
        shm_struct_ptr->user_pipe[idx][3] = fd;

    } else {
        cout << "Error: unknown purpose\n";
        return -1;
    }
    

    return fd;
}

void ShM::remove_id()
{
    shm_struct_ptr->id[client_id] = 0;
}

void ShM::remove_user_pipe(int src_id, int trg_id)
{
    for(int r=0; r<500; r++) {
        if(src_id == -1) {
            if(shm_struct_ptr->user_pipe[r][1] == trg_id) {
                shm_struct_ptr->user_pipe[r][0] = 0;
                shm_struct_ptr->user_pipe[r][1] = 0;
                shm_struct_ptr->user_pipe[r][2] = 0;
                shm_struct_ptr->user_pipe[r][3] = 0;
            }
        } else {
            if(shm_struct_ptr->user_pipe[r][0] == src_id && shm_struct_ptr->user_pipe[r][1] == trg_id) {
                shm_struct_ptr->user_pipe[r][0] = 0;
                shm_struct_ptr->user_pipe[r][1] = 0;
                shm_struct_ptr->user_pipe[r][2] = 0;
                shm_struct_ptr->user_pipe[r][3] = 0;
            }
        }
    }
}

string ShM::get_ip()
{
    string res = shm_struct_ptr->ip[client_id];
    return res;
}

string ShM::get_ip(int trg_id)
{
    string res = shm_struct_ptr->ip[trg_id];
    return res;
}

int ShM::get_port()
{
    return shm_struct_ptr->port[client_id];
}

int ShM::get_port(int trg_id)
{
    return shm_struct_ptr->port[trg_id];
}

int ShM::get_id()
{
    return client_id;
}

vector<int> ShM::get_all_id()
{
    vector<int> result;
    for(int i=1; i<100; i++) {
        if(shm_struct_ptr->id[i] != 0) {
            result.push_back(i);
        }
    }
    return result;
}

bool ShM::is_id_exist(int trg_id)
{
    return shm_struct_ptr->id[trg_id] != 0;
}

string ShM::get_name()
{
    string res = shm_struct_ptr->name[client_id];
    return res;
}

string ShM::get_name(int trg_id)
{
    string res = shm_struct_ptr->name[trg_id];
    return res;
}

vector<string> ShM::get_all_name()
{
    vector<string> vec;
    for(int i=1; i<100; i++) {
        if(shm_struct_ptr->id[i] != 0) {
            string tmp = shm_struct_ptr->name[i];
            vec.push_back(tmp);
        }
    }
    return vec;
}

bool ShM::is_name_exist(string trg_name)
{
    for(int i=1; i<100; i++) {
        if(shm_struct_ptr->id[i] != 0) {
            if(shm_struct_ptr->name[i] == trg_name) {
                return true;
            }
        }
    }
    return false;
}

string ShM::get_message()
{
    string res = shm_struct_ptr->message;
    return res;
}

string ShM::get_env()
{
    return shm_struct_ptr->env[client_id];
}

string ShM::get_env_value(string key)
{
    map<string, string> table = get_env_table();
    string result;
    if(table.find(key) != table.end()) {
        result = table[key];
    } else {
        result = "";
    }
    return result;
}

map<string, string> ShM::get_env_table()
{
    map<string, string> table;
    stringstream ss1(shm_struct_ptr->env[client_id]);
    string word;
    while (ss1 >> word) {
        stringstream ss2(word);
        string segment;
        vector<string> vec;
        while(getline(ss2, segment, '=')) {
            vec.push_back(segment);
        }
        table[vec[0]] = vec[1];
    }
    return table;
}

bool ShM::is_user_pipe_exist(int src_id, int trg_id)
{
    for(int i=0; i<500; i++) {
        if(shm_struct_ptr->user_pipe[i][0] == src_id && shm_struct_ptr->user_pipe[i][1] == trg_id) {
            return true;
        }
    }

    return false;
}

int ShM::get_user_pipe_fd(int src_id, int trg_id)
{
    int result;
    for(int i=0; i<500; i++) {
        if(shm_struct_ptr->user_pipe[i][0] == src_id && shm_struct_ptr->user_pipe[i][1] == trg_id) {
            result = shm_struct_ptr->user_pipe[i][3];
            break;
        }
    }
    return result;
}

void ShM::signal_user(int user_id)
{
    if(user_id != -1) {
        int pid = shm_struct_ptr->id[user_id];
        kill(pid, SIGUSR1);
    } else {
        // all user (broadcast)
        for(int i=1; i<100; i++) {
            if(shm_struct_ptr->id[i] != 0) {
                kill(shm_struct_ptr->id[i], SIGUSR1);
            }
        }
    }
    
}