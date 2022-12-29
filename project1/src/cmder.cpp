#include "cmder.h"

CMDer::CMDer()
{
    // register SIGCHLD handler
    if(signal(SIGCHLD, sigchld_handler)) {
       user_io.err_sys("Cannot register SGICHLD handler"); 
    }

    cmd_uid = -1;
    cmdline_uid = -1;
}

int CMDer::exec(vector<string> cmd_vec)
{
    cmdline_uid++;

    // non-pipe command
    if(is_simple_cmd(cmd_vec)) {
        // built-in command
        if(cmd_vec[0] == "setenv" || cmd_vec[0] == "printenv") {
            cmd_uid++;
            exec_built_in(cmd_vec);
        } else if(cmd_vec[0] == "exit") {
            return 0;
        }

        // single bin command
        else {
            cmd_uid++;

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

            // ordinary pipe 
            if(pipe_type[i] == 0) {
                int target_cmd_uid = cmd_uid + 1;            
                cmd_r_pipe_table[target_cmd_uid] = cmd_uid;
            }

            // number pipe
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


void CMDer::exec_built_in(vector<string> cmd_vec)
{
   if(cmd_vec[0] == "setenv") {
       setenv(cmd_vec[1].c_str(), cmd_vec[2].c_str(), true);
   } else {
       string msg = "";
       char* res = getenv(cmd_vec[1].c_str());
       if(res != nullptr) {
           msg = res;
       }
       if(msg.length() != 0) {
           msg += "\n";
       }
       user_io.output(msg);
   }
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
        if(explicit_wait || program_path == ""){
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
                dup2(1, 2);
                close(fd_err);
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
