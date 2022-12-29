#include "cmder.h"

CMDer::CMDer()
{
    // register SIGCHLD handler
    // if(signal(SIGCHLD, sigchld_handler)) {
    //    user_io.err_sys("Cannot register SGICHLD handler"); 
    // }

    cmd_uid = -1;
    cmdline_uid = -1;

    
    server_type = "";
    socket_fd = -1;

    client_ip_port_table = NULL;
    client_name_table = NULL;
    client_id_table = NULL;
    client_user_pipe_table = NULL;
    client_env_table = NULL;

    client_shmer = NULL;
}

int CMDer::exec(vector<string> cmd_vec, string raw_cmd)
{
    // set environment variable for this user
    if(server_type == "single_process_server") {
        map<string, string>::iterator iter;
        for(iter=(*client_env_table)[this->socket_fd].begin(); iter!=(*client_env_table)[this->socket_fd].end(); iter++) {
            setenv((iter->first).c_str(), (iter->second).c_str(), true);
        }
    } else if(server_type == "multi_process_server") {
        map<string, string> table = client_shmer->get_env_table();
        map<string, string>::iterator iter;
        for(iter=table.begin(); iter!=table.end(); iter++) {
            setenv((iter->first).c_str(), (iter->second).c_str(), true);
        }
    }
    
    this->raw_cmd = raw_cmd;

    cmdline_uid++;  

    // non-pipe command
    if(is_simple_cmd(cmd_vec)) {
        cmd_uid++;

        // built-in command
        if(cmd_vec.size() == 3 && cmd_vec[0] == "setenv") {
            if(server_type == "concurrent_server") {
                setenv(cmd_vec[1].c_str(), cmd_vec[2].c_str(), true);
            } else if(server_type == "single_process_server") {
                (*client_env_table)[this->socket_fd][cmd_vec[1]] = cmd_vec[2];
            } else if(server_type == "multi_process_server") {
                client_shmer->set_env(cmd_vec[1], cmd_vec[2]);
            }
        } else if(cmd_vec.size() == 2 && cmd_vec[0] == "printenv") {
            if(server_type == "concurrent_server") {
                string msg = "";
                char* res = getenv(cmd_vec[1].c_str());
                if(res != nullptr) {
                    msg = res;
                }
                if(msg.length() != 0) {
                    msg += "\n";
                }
                user_io.output(msg);
            } else if(server_type == "single_process_server") {
                map<string, string> table = (*client_env_table)[this->socket_fd];
                string msg;
                if(table.find(cmd_vec[1]) != table.end()) {
                    msg = table[cmd_vec[1]];
                } else {
                    msg = "";
                }
                if(msg.length() != 0) {
                    msg += "\n";
                }
                user_io.output(msg);
            } else if(server_type == "multi_process_server") {
                string msg = client_shmer->get_env_value(cmd_vec[1]);
                if(msg.length() != 0) {
                    msg += "\n";
                }
                user_io.output(msg);
            }
        } else if(cmd_vec.size() == 2 && cmd_vec[0] == "yell") {
            if(server_type == "single_process_server") {
                string msg = "*** " + (*client_name_table)[socket_fd] + " yelled ***: " + cmd_vec[1] + "\n";
                broadcast_msg(msg);
            } else if(server_type == "multi_process_server") {
                string msg = "*** " + client_shmer->get_name() + " yelled ***: " + cmd_vec[1] + "\n";
                client_shmer->set_message(msg);
                client_shmer->signal_user(-1);
            }
        } else if(cmd_vec.size() == 2 && cmd_vec[0] == "name") {
            if(server_type == "single_process_server") {
                // check if new name is exist
                bool name_exist = false;
                map<int, string>::iterator iter;
                for(iter=client_name_table->begin(); iter!=client_name_table->end(); iter++) {
                    if(iter->second == cmd_vec[1]) {
                        name_exist = true;
                        break;
                    }
                }

                if(name_exist) {
                    string msg = "*** User '" + cmd_vec[1] + "' already exists. ***\n";
                    cout << msg;
                } else {
                    // change user name in table
                    (*client_name_table)[socket_fd] = cmd_vec[1];

                    // boardcast message
                    string msg = "*** User from " + (*client_ip_port_table)[socket_fd].first + ":" + to_string((*client_ip_port_table)[socket_fd].second) + " is named '" + cmd_vec[1] + "'. ***\n";
                    broadcast_msg(msg);
                }
            } else if(server_type == "multi_process_server") {
                bool name_exist = client_shmer->is_name_exist(cmd_vec[1]);
                if(name_exist) {
                    string msg = "*** User '" + cmd_vec[1] + "' already exists. ***\n";
                    cout << msg;
                } else {
                    // change user name in table
                    client_shmer->set_name(cmd_vec[1]);

                    // boardcast message
                    string msg = "*** User from " + client_shmer->get_ip() + ":" + to_string(client_shmer->get_port()) + " is named '" + cmd_vec[1] + "'. ***\n";
                    client_shmer->set_message(msg);
                    client_shmer->signal_user(-1);
                }
            }
        } else if(cmd_vec.size() == 3 && cmd_vec[0] == "tell") {
            if(server_type == "single_process_server") {
                // check if target user id exist
                int target_id = stoi(cmd_vec[1]);
                if(client_id_table->find(target_id) != client_id_table->end()) {
                    // back up original stdout
                    int original_stdout_ = dup(1);

                    // redirect stdout to target socket
                    dup2((*client_id_table)[target_id], 1);

                    // send message
                    string msg = "*** " + (*client_name_table)[socket_fd] + " told you ***: " + cmd_vec[2] + "\n";
                    cout << msg;

                    // restore original stdout
                    dup2(original_stdout_, 1);
                    close(original_stdout_);
                } else {
                    string msg = "*** Error: user #" + cmd_vec[1] + " does not exist yet. ***" + "\n";
                    cout << msg;
                }
            } else if(server_type == "multi_process_server") {
                int target_id = stoi(cmd_vec[1]);
                if(client_shmer->is_id_exist(target_id)) {
                    string msg = "*** " + client_shmer->get_name() + " told you ***: " + cmd_vec[2] + "\n";
                    client_shmer->set_message(msg);
                    client_shmer->signal_user(target_id);
                } else {
                    string msg = "*** Error: user #" + cmd_vec[1] + " does not exist yet. ***" + "\n";
                    cout << msg;
                }
            }
        } else if(cmd_vec.size() == 1 && cmd_vec[0] == "who") {
            if(server_type == "single_process_server") {
                cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
                map<int, int>::iterator iter;
                for(iter=client_id_table->begin(); iter!=client_id_table->end(); iter++) {
                    cout << iter->first << "\t";
                    cout << (*client_name_table)[iter->second] << "\t";
                    cout << (*client_ip_port_table)[iter->second].first << ":" << (*client_ip_port_table)[iter->second].second;
                    if(iter->second == socket_fd) {
                        cout << "\t<-me";
                    }
                    cout << "\n";
                }
            } else if(server_type == "multi_process_server") {
                cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
                vector<int> all_id = client_shmer->get_all_id();
                for(int i=0; i<all_id.size(); i++) {
                    cout << all_id[i] << "\t";
                    cout << client_shmer->get_name(all_id[i]) << "\t";
                    cout << client_shmer->get_ip(all_id[i]) << ":" << client_shmer->get_port(all_id[i]);
                    if(all_id[i] == client_shmer->get_id()) {
                        cout << "\t<-me";
                    }
                    cout << "\n";
                }
            }
        } else if(cmd_vec[0] == "exit") {
            return 0;
        }

        // single bin command
        else {
            // check if previous cmd number pipe to this cmd
            if(cmdline_r_pipe_table.find(cmdline_uid) != cmdline_r_pipe_table.end()) {
                int key = cmdline_r_pipe_table[cmdline_uid];
                cmdline_r_pipe_table.erase(cmdline_uid);
                int fd_in = cmd_pipe_table[key];
                exec_bin(
                    cmd_vec, 
                    true,
                    true,
                    fd_in,
                    1,
                    2,
                    false,
                    ""
                );
            } else {
                exec_bin(
                    cmd_vec, 
                    true,
                    false,
                    -1,
                    -1,
                    -1,
                    false,
                    ""
                );
            }
        }

    }
    
    // pipe command
    else {
        vector< vector<string> > cmds_vec;
        vector<int> pipe_num;
        vector<int> pipe_type;
        parse_pipe_cmd(cmd_vec, cmds_vec, pipe_num, pipe_type);

        int fd_in, fd_err, fd_pipe[2];
        
        bool pipe_conflict;
        string pipe_conflict_content;

        // last command is not processed in this loop
        for(int i=0; i<cmds_vec.size()-1; i++) {
            cmd_uid++;
            pipe_conflict = false;

            // create pipe
            pipe(fd_pipe);
            
            // update cmd_pipe_table
            cmd_pipe_table[cmd_uid] = fd_pipe[0];
            
            // decide fd_in from cmd_r_pipe_table and cmdline_r_pipe_table
            
            // if ordinary pipe to this cmd
            if(cmd_r_pipe_table.find(cmd_uid) != cmd_r_pipe_table.end()) {
                int key = cmd_r_pipe_table[cmd_uid];
                fd_in = cmd_pipe_table[key];
            }

            // no ordinary pipe to this cmd, then check if number pipe to this cmd
            else if(cmdline_r_pipe_table.find(cmdline_uid) != cmdline_r_pipe_table.end()) {
                int key = cmdline_r_pipe_table[cmdline_uid];
                cmdline_r_pipe_table.erase(cmdline_uid);
                fd_in = cmd_pipe_table[key];
            }

            // decide fd_err from pipe_type
            if (pipe_type[i] == 0 || pipe_type[i] == 1) {
                fd_err = 2;
            } else if (pipe_type[i] == 2) {
                fd_err = fd_pipe[1];
            }
            
            // update cmd_r_pipe_table and 
            // cmdline_r_pipe_table for later cmd

            // ordinary pipe (ex. |)
            if(pipe_type[i] == 0) {
                int target_cmd_uid = cmd_uid + 1;            
                cmd_r_pipe_table[target_cmd_uid] = cmd_uid;
            }

            // number pipe (ex. |2)
            else {
                int target_cmdline_uid = cmdline_uid + pipe_num[i];

                // alread one cmd number pipe to same cmd line
                if(cmdline_r_pipe_table.find(target_cmdline_uid) != cmdline_r_pipe_table.end()) {
                
                    // get original pipe
                    int original_pipe = cmdline_r_pipe_table[target_cmdline_uid];
                    original_pipe = cmd_pipe_table[original_pipe];

                    // read data from original pipe
                    int nbytes;
                    char one_char;
                    pipe_conflict_content = "";
                    while ((nbytes = read(original_pipe, &one_char, 1)) > 0) {
                        pipe_conflict_content += one_char;
                    }
                    pipe_conflict = true;
                }

                cmdline_r_pipe_table[target_cmdline_uid] = cmd_uid;

            }

            // exec_bin
            exec_bin(
                cmds_vec[i],
                false,
                true,
                fd_in,
                fd_pipe[1],
                fd_err,
                pipe_conflict,
                pipe_conflict_content
            );
            
            // close pipe's write end
            close(fd_pipe[1]);

            // if current cmd is number pipe, next cmd is seen at next line
            if(pipe_type[i] == 1 || pipe_type[i] == 2) {
                cmdline_uid++;
            }
        }


        // handle last cmd
        cmd_uid++;
        pipe_conflict = false;
        
        // decide fd_in from cmd_r_pipe_table and cmdline_r_pipe_table
            
        // if ordinary pipe to this cmd
        if(cmd_r_pipe_table.find(cmd_uid) != cmd_r_pipe_table.end()) {
            int key = cmd_r_pipe_table[cmd_uid];
            fd_in = cmd_pipe_table[key];
        }

        // no ordinary pipe to this cmd, then check if number pipe to this cmd
        else if(cmdline_r_pipe_table.find(cmdline_uid) != cmdline_r_pipe_table.end()) {
            int key = cmdline_r_pipe_table[cmdline_uid];
            cmdline_r_pipe_table.erase(cmdline_uid);
            fd_in = cmd_pipe_table[key];
        }

        // decide fd_err from pipe_type
        if (pipe_type.back() == 0 || pipe_type.back() == 1) {
            fd_err = 2;
        } else if (pipe_type.back() == 2) {
            // any number which is not 4 is OK
            fd_err = 4;
        }

        // last command with pipe (must be number pipe)
        if(pipe_num.back() != 0) {

            pipe(fd_pipe);
            cmd_pipe_table[cmd_uid] = fd_pipe[0];
            
            int target_cmdline_uid = cmdline_uid + pipe_num.back();            

            // alread one cmd number pipe to same cmd line
            if(cmdline_r_pipe_table.find(target_cmdline_uid) != cmdline_r_pipe_table.end()) {
                
                // get original pipe
                int original_pipe = cmdline_r_pipe_table[target_cmdline_uid];
                original_pipe = cmd_pipe_table[original_pipe];

                // read data from original pipe
                int nbytes;
                char one_char;
                pipe_conflict_content = "";
                while ((nbytes = read(original_pipe, &one_char, 1)) > 0) {
                    pipe_conflict_content += one_char;
                }

                pipe_conflict = true;
            }
            
            cmdline_r_pipe_table[target_cmdline_uid] = cmd_uid;
        }



        if(pipe_num.back() != 0) {
            exec_bin(
                cmds_vec.back(),
                false,
                true,
                fd_in,
                fd_pipe[1],
                fd_err,
                pipe_conflict,
                pipe_conflict_content
            );
            close(fd_pipe[1]);
        } else {
            exec_bin(
                cmds_vec.back(),
                true,
                true,
                fd_in,
                1,
                fd_err,
                pipe_conflict,
                pipe_conflict_content
            );
        }
    }

    return 1;
}


bool CMDer::is_simple_cmd(vector<string> cmd_vec) 
{
    bool pipe = false;
    bool number_pipe = false;

    // check pipe
    for(int i=0; i<cmd_vec.size(); i++) {
        if(cmd_vec[i] == "|") {
            pipe = true;
            break;
        }
    }

    // check number pipe
    regex pattern1("\\|\\d+");
    regex pattern2("\\!\\d+");
    for(int i=0; i<cmd_vec.size(); i++) {
        if(regex_search(cmd_vec[i], pattern1) || regex_search(cmd_vec[i], pattern2)) {
            number_pipe= true;
            break;
        }
    }

    return !(pipe || number_pipe);
}

string CMDer::get_cmd_full_path(vector<string> cmd_vec)
{
    // find program of cmd
    vector<string> paths = user_io.string_split(getenv("PATH"), ":");
    string find_path;

    for(int i=0; i<paths.size(); i++) {
        // create full path of bin program
        filesystem::path dir(paths[i]);
        filesystem::path file(cmd_vec[0]);
        filesystem::path full_path = dir / file;

        // check if program exists
        filesystem::path program(full_path);
        if(filesystem::exists(program)) {
            find_path = full_path;
            break;
        }
    }

    return find_path;
}


void CMDer::sigchld_handler(int signo)
{
    wait(nullptr);
}

void CMDer::exec_bin(
    vector<string> cmd_vec,
    bool explicit_wait,
    bool pipe_cmd,
    int fd_in,
    int fd_out,
    int fd_err,
    bool pipe_conflict,
    string pipe_conflict_content
) 
{
    /*
        a table record user pipe info:
        <string> => <int>
        "input" => 1
        "output" => 5
    */
    map<string, int> user_pipe_info;
    int fd_user_pipe[2], fd_in_user_pipe, fd_out_user_pipe;
    bool is_user_pipe_cmd = false;
    

    // check if cmd_vec contains '>3' or '<5'
    if(contain_user_pipe(cmd_vec)) {
        // fill in info in user_pipe_table, and return 'clean' cmd_vec without user pipe
        cmd_vec = parse_user_pipe(cmd_vec, &user_pipe_info);

        // user pipe (input)
        if(user_pipe_info["input"] != -1) {
            if(server_type == "single_process_server") {
                int src_user_id = user_pipe_info["input"];
                int trg_user_id;
                for(map<int, int>::iterator iter=client_id_table->begin(); iter!=client_id_table->end(); iter++) {
                    if(iter->second == socket_fd) {
                        trg_user_id = iter->first;
                        break;
                    }
                }

                // check if source user exists
                if(client_id_table->find(src_user_id) == client_id_table->end()) {
                    string msg = "*** Error: user #" + to_string(src_user_id) + " does not exist yet. ***\n";
                    cout << msg;
                    
                    // error handling
                    fd_in_user_pipe = open("/dev/null", O_RDWR);
                } else {
                    // check if (source user => target user) exists
                    if(client_user_pipe_table->find(make_pair(src_user_id, trg_user_id)) == client_user_pipe_table->end()) {
                        string msg = "*** Error: the pipe #" + to_string(src_user_id) + "->#" + to_string(trg_user_id) + " does not exist yet. ***\n";
                        cout << msg;

                        // error handling
                        fd_in_user_pipe = open("/dev/null", O_RDWR);
                    } else {
                        // remember read end of pipe
                        fd_in_user_pipe = (*client_user_pipe_table)[make_pair(src_user_id, trg_user_id)];

                        // remove entry in client_user_pipe_table
                        client_user_pipe_table->erase(make_pair(src_user_id, trg_user_id));

                        // broadcast message
                        string msg = "*** " + (*client_name_table)[this->socket_fd] + " (#" + to_string(trg_user_id) +") just received from " + (*client_name_table)[(*client_id_table)[src_user_id]] + " (#" + to_string(src_user_id) + ") by '" + this->raw_cmd + "' ***\n";
                        broadcast_msg(msg);
                    }
                }
            } else if(server_type == "multi_process_server") {
                int src_user_id = user_pipe_info["input"];
                int trg_user_id = client_shmer->get_id();

                // check if source user exists
                if(client_shmer->is_id_exist(src_user_id) == false) {
                    string msg = "*** Error: user #" + to_string(src_user_id) + " does not exist yet. ***\n";
                    cout << msg;
                    
                    // error handling
                    fd_in_user_pipe = open("/dev/null", O_RDWR);
                } else {
                    // check if (source user => target user) exists
                    if(client_shmer->is_user_pipe_exist(src_user_id, trg_user_id) == false) {
                        string msg = "*** Error: the pipe #" + to_string(src_user_id) + "->#" + to_string(trg_user_id) + " does not exist yet. ***\n";
                        cout << msg;

                        // error handling
                        fd_in_user_pipe = open("/dev/null", O_RDWR);
                    } else {
                        // remember read end of pipe
                        fd_in_user_pipe = client_shmer->get_user_pipe_fd(src_user_id, trg_user_id);

                        // remove user pipe info from shared memory
                        client_shmer->remove_user_pipe(src_user_id, trg_user_id);

                        // broadcast message
                        string msg = "*** " + client_shmer->get_name() + " (#" + to_string(trg_user_id) +") just received from " + client_shmer->get_name(src_user_id) + " (#" + to_string(src_user_id) + ") by '" + this->raw_cmd + "' ***\n";
                        client_shmer->set_message(msg);
                        client_shmer->signal_user(-1);
                    }
                }
            }
        }

        // user pipe (output)
        if(user_pipe_info["output"] != -1) {
            if(server_type == "single_process_server") {
                int src_user_id;
                for(map<int, int>::iterator iter=client_id_table->begin(); iter!=client_id_table->end(); iter++) {
                    if(iter->second == socket_fd) {
                        src_user_id = iter->first;
                        break;
                    }
                }
                int trg_user_id = user_pipe_info["output"];

                // check if target user exists
                if(client_id_table->find(trg_user_id) == client_id_table->end()) {
                    string msg = "*** Error: user #" + to_string(trg_user_id) + " does not exist yet. ***\n";
                    cout << msg;
                    
                    // error handling
                    fd_out_user_pipe = open("/dev/null", O_RDWR);
                } else {
                    // check if (source user => target user) exists
                    if(client_user_pipe_table->find(make_pair(src_user_id, trg_user_id)) != client_user_pipe_table->end()) {
                        string msg = "*** Error: the pipe #" + to_string(src_user_id) + "->#" + to_string(trg_user_id) + " already exists. ***\n";
                        cout << msg;

                        // error handling
                        fd_out_user_pipe = open("/dev/null", O_RDWR);
                    } else {
                        // create new pipe
                        pipe(fd_user_pipe);

                        // remember write end of pipe
                        fd_out_user_pipe = fd_user_pipe[1];

                        // add entry to client_user_pipe_table
                        (*client_user_pipe_table)[make_pair(src_user_id, trg_user_id)] = fd_user_pipe[0];

                        // boradcast meessage
                        string msg = "*** " + (*client_name_table)[socket_fd] + " (#" + to_string(src_user_id) + ") just piped '" + this->raw_cmd + "' to " + (*client_name_table)[(*client_id_table)[trg_user_id]] + " (#" + to_string(trg_user_id) + ") ***\n";
                        broadcast_msg(msg);
                    }
                }
            } else if(server_type == "multi_process_server") {
                int src_user_id = client_shmer->get_id();
                int trg_user_id = user_pipe_info["output"];

                // check if target user exists
                if(client_shmer->is_id_exist(trg_user_id) == false) {
                    string msg = "*** Error: user #" + to_string(trg_user_id) + " does not exist yet. ***\n";
                    cout << msg;
                    
                    // error handling
                    fd_out_user_pipe = open("/dev/null", O_RDWR);
                } else {
                    // check if (source user => target user) exists
                    if(client_shmer->is_user_pipe_exist(src_user_id, trg_user_id)) {
                        string msg = "*** Error: the pipe #" + to_string(src_user_id) + "->#" + to_string(trg_user_id) + " already exists. ***\n";
                        cout << msg;

                        // error handling
                        fd_out_user_pipe = open("/dev/null", O_RDWR);
                    } else {
                        // create new pipe
                        fd_out_user_pipe = client_shmer->set_user_pipe(src_user_id, trg_user_id, "write");

                        // boradcast meessage
                        string msg = "*** " + client_shmer->get_name() + " (#" + to_string(src_user_id) + ") just piped '" + this->raw_cmd + "' to " + client_shmer->get_name(trg_user_id) + " (#" + to_string(trg_user_id) + ") ***\n";
                        client_shmer->set_message(msg);
                        client_shmer->signal_user(-1);
                    }
                }
            }
        }

        is_user_pipe_cmd = (user_pipe_info["input"] != -1) || (user_pipe_info["output"] != -1);
    }

    string program_path = get_cmd_full_path(cmd_vec);

    // fork new process
    pid_t pid;
    while((pid = fork()) == -1) {
        // if too many process are created, wait for one second
        sleep(1);
    }

    // child process
    if(pid == 0) {
        if(program_path == "") {
            string msg = "Unknown command: [" + cmd_vec[0] + "]." + "\n";
            user_io.outerr(msg);
            exit(0);
        }
    } 
    
    // parent process
    else {
        // close write end of user pipe
        if(is_user_pipe_cmd && user_pipe_info["output"] != -1) {
            explicit_wait = false;
            close(fd_out_user_pipe);
        }

        // close read end of user pipe
        if(is_user_pipe_cmd && user_pipe_info["input"] != -1) {
            close(fd_in_user_pipe);
        }

        if(explicit_wait || program_path == ""){
            // parent process will explicitly wait child process
            // child process should not be handled by SGCHLD handler
            signal(SIGCHLD, SIG_DFL);

            waitpid(pid, nullptr, 0);
        }
    }
    
    // child process
    if(pid == 0) {
        
        // redirect output
        int redirect_idx = -1;
        for(int i=0; i<cmd_vec.size(); i++) {
            if(cmd_vec[i] == ">") {
                redirect_idx = i;
                break;
            }
        }
        
        if(redirect_idx != -1) {
            int fd = creat(cmd_vec[redirect_idx+1].c_str(), 0644);
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        
        // pipe cmd
        if(pipe_cmd) {
            if(fd_in != 0) {
                dup2(fd_in, 0);
                close(fd_in);
            }
            
            if(fd_out != 1) {
                dup2(fd_out, 1);
                close(fd_out);
            }
            
            if(fd_err != 2) {
                // display error
                dup2(1, 2);
                close(fd_err);
            }
        }

        // user pipe cmd
        if(is_user_pipe_cmd) {
            // input
            if(user_pipe_info["input"] != -1) {
                dup2(fd_in_user_pipe, 0);
                close(fd_in_user_pipe);
            }

            // output
            if(user_pipe_info["output"] != -1) {
                dup2(fd_out_user_pipe, 1);
                close(fd_out_user_pipe);
            }
        }

        // pipe conflict
        if(pipe_cmd && pipe_conflict) {
            write(1, pipe_conflict_content.c_str(), pipe_conflict_content.length());
        }

        // exec to load new program
        char* args[100];
        int i;
        for(i=0; i<cmd_vec.size(); i++) {
            // ignore string after output redirect symbol: '>'
            if(cmd_vec[i] == ">") {
                break;
            }
            args[i] = cmd_vec[i].data();
        }
        args[i] = NULL;
        execv(program_path.c_str(), args);

        // end of child process
        exit(0);
    }
}

void CMDer::parse_pipe_cmd(
    vector<string> cmd_vec, 
    vector< vector<string> > & cmds_vec, 
    vector<int> & pipe_num,
    vector<int> & pipe_type
)
{
    /*
        pipe type definition:

        original pipe (|) => 0
        number pipe (pattern 1) (|3) => 1
        number pipe (pattern 2) (!4) => 2
    */

    // number pipe pattern
    regex pattern1("\\|(\\d+)");
    regex pattern2("\\!(\\d+)");
    smatch reg_match;
    int prev_idx = 0;

    for(int i=0; i<cmd_vec.size(); i++) {
        // pipe
        if(cmd_vec[i] == "|") {
            vector<string> tmp;
            for(int j=prev_idx; j<i; j++) {
                tmp.push_back(cmd_vec[j]);
            }
            cmds_vec.push_back(tmp);
            pipe_num.push_back(1);
            pipe_type.push_back(0);
            prev_idx = i+1;
        }

        // number pipe (pattern 1)
        else if(regex_search(cmd_vec[i], reg_match, pattern1)) {
            string num_str = reg_match[1].str();
            int num = stoi(num_str);
            vector<string> tmp;
            for(int j=prev_idx; j<i; j++) {
                tmp.push_back(cmd_vec[j]);
            }
            cmds_vec.push_back(tmp);
            pipe_num.push_back(num);
            pipe_type.push_back(1);
            prev_idx = i+1;
        }
        
        // number pipe (pattern 2)
        else if(regex_search(cmd_vec[i], reg_match, pattern2)) {
            string num_str = reg_match[1].str();
            int num = stoi(num_str);
            vector<string> tmp;
            for(int j=prev_idx; j<i; j++) {
                tmp.push_back(cmd_vec[j]);
            }
            cmds_vec.push_back(tmp);
            pipe_num.push_back(num);
            pipe_type.push_back(2);
            prev_idx = i+1;
        }
    }

    // last command without pipe or number pipe appended
    if(prev_idx != cmd_vec.size()) {
        vector<string> tmp;
        for(int j=prev_idx; j<cmd_vec.size(); j++) {
            tmp.push_back(cmd_vec[j]);
        }
        cmds_vec.push_back(tmp);
        pipe_num.push_back(0);
        pipe_type.push_back(0);
    }
}



void CMDer::set_client_table(
    map<int, pair<string, int> >* ip_port_table,
    map<int, string>* name_table,
    map<int, int>* id_table,
    map<pair<int, int>, int>* user_pipe_table,
    map<int, map<string, string> >* env_table
)
{
    client_ip_port_table = ip_port_table;
    client_name_table = name_table;
    client_id_table = id_table;
    client_user_pipe_table = user_pipe_table;
    client_env_table = env_table;
}

void CMDer::set_client_shmer(ShM* shmer)
{
    client_shmer = shmer;
}

void CMDer::set_socket_fd(int fd)
{
    socket_fd = fd;
}

void CMDer::set_server_type(string type_name)
{
    server_type = type_name;
}

void CMDer::broadcast_msg(string msg)
{
    // back up original stdout
    int original_stdout = dup(1);

    // iterate all client
    map<int, pair<string, int> >::iterator iter;
    for(iter=client_ip_port_table->begin(); iter!=client_ip_port_table->end(); iter++) {
        dup2(iter->first, 1);
        cout << msg;
    }

    // restore original stdout
    dup2(original_stdout, 1);
    close(original_stdout);
}

bool CMDer::contain_user_pipe(vector<string> cmd_vec)
{
    // check user pipe
    regex pattern1("\\<\\d+");
    regex pattern2("\\>\\d+");
    for(int i=0; i<cmd_vec.size(); i++) {
        if(regex_search(cmd_vec[i], pattern1) || regex_search(cmd_vec[i], pattern2)) {
            return true;
        }
    }

    return false;
}

vector<string> CMDer::parse_user_pipe(vector<string> cmd_vec, map<string, int>* user_pipe_info)
{
    regex pattern1("\\<(\\d+)");
    regex pattern2("\\>(\\d+)");
    smatch reg_match;
    vector<string> new_cmd_vec;

    for(int i=0; i<cmd_vec.size(); i++) {

        // user pipe (input)
        if(regex_search(cmd_vec[i], reg_match, pattern1)) {
            string num_str = reg_match[1].str();
            int num = stoi(num_str);
            (*user_pipe_info)["input"] = num;
            continue;
        }

        // user pipe (output)
        if(regex_search(cmd_vec[i], reg_match, pattern2)) {
            string num_str = reg_match[1].str();
            int num = stoi(num_str);
            (*user_pipe_info)["output"] = num;
            continue;
        }

        new_cmd_vec.push_back(cmd_vec[i]);
    }

    if(user_pipe_info->find("input") == user_pipe_info->end()) {
        (*user_pipe_info)["input"] = -1;
    }

    if(user_pipe_info->find("output") == user_pipe_info->end()) {
        (*user_pipe_info)["output"] = -1;
    }

    return new_cmd_vec;
}