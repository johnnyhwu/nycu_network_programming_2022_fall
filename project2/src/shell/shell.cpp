#include "shell.h"

Shell::Shell()
{
    user_io = new UserIO(true);
    cmder = new CMDer();

    client_ip_port_table = NULL;
    client_name_table = NULL;
    client_id_table = NULL;
    client_user_pipe_table = NULL;
    client_env_table = NULL;

    client_shmer = NULL;
}

void Shell::activate(
    string server_type, 
    int socket_fd,

    map<int, pair<string, int> >* ip_port_table,
    map<int, string>* name_table,
    map<int, int>* id_table,
    map<pair<int, int>, int>* user_pipe_table,
    map<int, map<string, string> >* env_table,

    ShM* shmer
)
{
    cmder->set_server_type(server_type);
    
    if(server_type == "concurrent_server") {
        activate_cc(socket_fd);
    } else if(server_type == "single_process_server") {

        // store all client information in 'Shell' object
        client_ip_port_table = ip_port_table;
        client_name_table = name_table;
        client_id_table = id_table;
        client_user_pipe_table = user_pipe_table;
        client_env_table = env_table;

        // store all client information in 'CMDer' object
        cmder->set_client_table(
            ip_port_table,
            name_table,
            id_table,
            user_pipe_table,
            env_table
        );
        cmder->set_socket_fd(socket_fd);

        // redirect standard output to socket
        int original_stdout = dup(1);
        dup2(socket_fd, 1);

        // welcome message
        cout << "****************************************" << "\n";
        cout << "** Welcome to the information server. **" << "\n";
        cout << "****************************************" << "\n";

        // boradcast message
        string msg = "*** User '" + (*name_table)[socket_fd] + "' entered from " + (*ip_port_table)[socket_fd].first + ":" + to_string((*ip_port_table)[socket_fd].second) + ". ***\n";
        broadcast_msg(msg);

        // initial command prompt
        dup2(socket_fd, 1);
        cout << "% ";

        // recover standard output
        dup2(original_stdout, 1);
        close(original_stdout);
    } else if(server_type == "multi_process_server") {

        // store all client information in 'Shell' object
        client_shmer = shmer;

        // store all client information in 'CMDer' object
        cmder->set_client_shmer(shmer);

        activate_mp(socket_fd);
    }
    
}

void Shell::activate_cc(int socket_fd)
{
    dup2(socket_fd, 0);
    dup2(socket_fd, 1);
    dup2(socket_fd, 2);

    // initialize
    vector<string> cmd_vec;
    
    while(true) {
        // display prompt
        user_io->output("% ");
        
        // wait for user input
        cmd_vec = user_io->input();

        // if user does not input anything
        if(cmd_vec.size() == 0) {
            continue;
        }

        // parse and execute user command
        if(cmder->exec(cmd_vec, user_io->get_raw_cmd()) == 0) {
            break;
        }
    }
}

int Shell::activate_sp(int socket_fd)
{
    // back up original standard file descriptor
    int original_stdin = dup(0);
    int original_stdout = dup(1);
    int original_stderr = dup(2);

    // redirect standard file descriptor to socket
    dup2(socket_fd, 0);
    dup2(socket_fd, 1);
    dup2(socket_fd, 2);

    // read form socket
    vector<string> cmd_vec;
    cmd_vec = user_io->input();
    int exec_result = 1;

    if(cmd_vec.size() != 0) {
        exec_result = cmder->exec(cmd_vec, user_io->get_raw_cmd());
    }

    if(exec_result != 0) {
        // display prompt
        user_io->output("% ");
    }
    
    // recover original standard file descriptor
    dup2(original_stdin, 0);
    dup2(original_stdout, 1);
    dup2(original_stderr, 2);

    close(original_stdin);
    close(original_stdout);
    close(original_stderr);

    return exec_result;
}

void Shell::activate_mp(int socket_fd)
{
    dup2(socket_fd, 0);
    dup2(socket_fd, 1);
    dup2(socket_fd, 2);

    // welcome message
    cout << "****************************************" << "\n";
    cout << "** Welcome to the information server. **" << "\n";
    cout << "****************************************" << "\n";

    // boradcast message
    string msg = "*** User '" + client_shmer->get_name() + "' entered from " + client_shmer->get_ip() + ":" + to_string(client_shmer->get_port()) + ". ***\n";
    client_shmer->set_message(msg);
    client_shmer->signal_user(-1);

    // initialize
    vector<string> cmd_vec;
    
    while(true) {
        // display prompt
        user_io->output("% ");
        
        // wait for user input
        cmd_vec = user_io->input();

        // if user does not input anything
        if(cmd_vec.size() == 0) {
            continue;
        }

        // parse and execute user command
        if(cmder->exec(cmd_vec, user_io->get_raw_cmd()) == 0) {
            break;
        }
    }
}

void Shell::broadcast_msg(string msg)
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