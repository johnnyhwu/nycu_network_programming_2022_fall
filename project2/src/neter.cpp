#include "neter.h"

ShM NETer::shmer;

NETer::NETer()
{
    ;
}

void NETer::concurrent_server(string service)
{
    cout << "Info: run concurrent_server (task 1)" << "\n";
    
    struct sockaddr_in fsin; /* the address of a client */
    socklen_t alen; /* length of client's address */
    int msock; /* master server socket */   
    int ssock; /* slave server socket */

    // create socket
    msock = create_socket(service);
    if (msock == -1) {
        return;
    }
    cout << "Info: create_socket: " << msock << "\n";

    // register SIGCHLD signal handler
    signal(SIGCHLD, sigchld_handler);

    while (1) {
        alen = sizeof(fsin);
        
        // block until client connect
        ssock = accept(msock, (struct sockaddr *) &fsin, &alen);
        cout << "Info: accept_socket: " << ssock << "\n";


        // fork new process to handle new connection
        pid_t pid = fork();

        // child process
        if(pid == 0) {
            close(msock);
            init_shell(
                "concurrent_server", 
                ssock,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL
            );
            exit(0);
        }

        // parent process
        else {
            close(ssock);
        }
    }

}

void NETer::single_proc_server(string service)
{
    cout << "Info: run single_proc_server (task 2)" << "\n";

    // client information
    map<int, Shell*> client_shell_table; /* socket fd => pointer to shell object */
    map<int, pair<string, int> > client_ip_port_table; /* socket fd => [ip, port] */
    map<int, string> client_name_table; /* socket fd => name */
    map<int, int> client_id_table; /* client id => socket fd */
    map<pair<int, int>, int> client_user_pipe_table; /* (src_user id, trg_user id) => src_user pipe read end */
    map<int, map<string, string> > client_env_table; /* socket fd => environment variable table */
    
    struct sockaddr_in fsin; /* the address of a client */
    socklen_t alen; /* length of client's address */
    int msock; /* master server socket */   
    int ssock; /* slave server socket */
    
    fd_set ready_fd_set; /* ready file descriptor set */
    fd_set active_fd_set; /* active file descriptor set */
    int num_fd = 100; /* total number of file descriptors to be checked */

    // create socket
    msock = create_socket(service);
    if (msock == -1) {
        return;
    }

    // initialize file descriptor set
    FD_ZERO(&active_fd_set);
    FD_ZERO(&ready_fd_set);
    FD_SET(msock, &active_fd_set);

    while (1) {
        ready_fd_set = active_fd_set;

        cout << "Before select()" << "\n";
        if (select(num_fd, &ready_fd_set, NULL, NULL, NULL) < 0) {
            cout << "Error: cannot select on ready_fd_set !" << "\n";
            return;
        }

        if (FD_ISSET(msock, &ready_fd_set)) { 

            alen = sizeof(fsin);
            ssock = accept(msock, (struct sockaddr *)&fsin, &alen);
            if (ssock < 0) {
                cout << "Error: cannot accept on msock !" << "\n";
            }

            // new client connects to server
            client_ip_port_table[ssock] = make_pair(inet_ntoa(fsin.sin_addr), (int)ntohs(fsin.sin_port));
            client_name_table[ssock] = "(no name)";
            for(int i=1; i<=100; i++) {
                if(client_id_table.find(i) == client_id_table.end()) {
                    client_id_table[i] = ssock;
                    break;
                }
            }
            map<string, string> env_table;
            env_table["PATH"] = "bin:.";
            client_env_table[ssock] = env_table;
            Shell* client_shell = init_shell(
                "single_process_server", 
                ssock, 
                &client_ip_port_table,
                &client_name_table,
                &client_id_table,
                &client_user_pipe_table,
                &client_env_table,
                NULL
            );
            client_shell_table[ssock] = client_shell;

            FD_SET(ssock, &active_fd_set);
        }

        for (int fd=0; fd<num_fd; ++fd) {
            if (fd != msock && FD_ISSET(fd, &ready_fd_set)) {
                
                // this client already connects to server
                if(client_shell_table.find(fd) != client_shell_table.end()) {
                    int exec_result = client_shell_table[fd]->activate_sp(fd);

                    // client exits
                    if(exec_result == 0) {

                        // clear its entry in tables
                        if(client_ip_port_table.erase(fd) == 0) {
                            cout << "Error: client_ip_port_table doesn't have key = " << fd << "\n";
                        }
                        
                        string client_name = client_name_table[fd];
                        if(client_name_table.erase(fd) == 0) {
                            cout << "Error: client_name_table doesn't have key = " << fd << "\n";
                        }
                        
                        int key = 0;
                        for(map<int, int>::iterator iter=client_id_table.begin(); iter!=client_id_table.end(); iter++) {
                            if(iter->second == fd) {
                                key = iter->first;
                                break;
                            }
                        }
                        if(client_id_table.erase(key) == 0) {
                            cout << "Error: client_id_table doesn't have key = " << key << "\n";
                        }
                        
                        
                        delete client_shell_table[fd];
                        if(client_shell_table.erase(fd) == 0) {
                            cout << "Error: client_shell_table doesn't have key = " << fd << "\n";
                        }

                        vector< pair<int, int> > rm_pairs;
                        for(map<pair<int, int>, int>::iterator iter=client_user_pipe_table.begin(); iter!=client_user_pipe_table.end(); iter++) {
                            if(iter->first.second == key) {
                                rm_pairs.push_back(iter->first);
                            }
                        }
                        for(int i=0; i<rm_pairs.size(); i++) {
                            client_user_pipe_table.erase(rm_pairs[i]);
                        }

                        if(client_env_table.erase(fd) == 0) {
                            cout << "Error: client_env_table doesn't have key = " << fd << "\n";
                        }

                        // close socket file descriptor
                        close(fd);
                        FD_CLR(fd, &active_fd_set);

                        // boradcast user logout message to all client
                        int original_stdout = dup(1);
                        for(map<int, pair<string, int> >::iterator iter=client_ip_port_table.begin(); iter!=client_ip_port_table.end(); iter++) {
                            dup2(iter->first, 1);
                            cout << "*** User '" << client_name << "' left. ***" << "\n";
                        }
                        dup2(original_stdout, 1);
                        close(original_stdout);
                    }
                } 
                
                else {
                    cout << "Error: unexpedted client" << "\n";
                    return;
                }
            }
        }
    }
}

void NETer::multi_proc_server(string service)
{
    cout << "Info: run multi_proc_server (shared memory + fifo) (task 3)" << "\n";
    
    struct sockaddr_in fsin; /* the address of a client */
    socklen_t alen; /* length of client's address */
    int msock; /* master server socket */   
    int ssock; /* slave server socket */

    // create socket
    msock = create_socket(service);
    if (msock == -1) {
        return;
    }
    cout << "Info: create_socket: " << msock << "\n";

    // register signal handler
    signal(SIGCHLD, sigchld_handler);
    signal(SIGINT, sigint_handler);
    signal(SIGUSR1, sigusr1_handler);
    signal(SIGUSR2, sigusr2_handler);

    // create a fake socket file descriptor
    int socket_fd_fake = -1;

    // create shared memory
    shmer.create(true);

    while (1) {
        alen = sizeof(fsin);
        
        // block until client connect
        ssock = accept(msock, (struct sockaddr *) &fsin, &alen);
        cout << "Info: accept_socket: " << ssock << "\n";

        // fork new process to handle new connection
        pid_t pid = fork();

        // child process
        if(pid == 0) {
            // use own pid as socket_fd_fake
            socket_fd_fake = getpid();

            // close master socket fd
            close(msock);

            // create (existing) shared memory
            ShM* client_shmer = new ShM();
            client_shmer->create(false);

            // add info of this new client to shared memory

            // id
            client_shmer->set_id(socket_fd_fake);

            // ip
            client_shmer->set_ip(inet_ntoa(fsin.sin_addr));

            // port
            client_shmer->set_port((int)ntohs(fsin.sin_port));

            // name
            client_shmer->set_name("(no name)");

            // environment variable
            client_shmer->set_env("PATH", "bin:.");

            // create shell
            init_shell(
                "multi_process_server",
                ssock,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                client_shmer
            );

            // remove info of this new client from shared memory

            // id
            client_shmer->remove_id();

            // user pipe
            client_shmer->remove_user_pipe(-1, client_shmer->get_id());

            // boradcast user logout message to all client
            string msg = "*** User '" + client_shmer->get_name() + "' left. ***\n";
            client_shmer->set_message(msg);
            client_shmer->signal_user(-1);

            // detach from shared memory
            client_shmer->detach();

            // delete object
            delete client_shmer;

            exit(0);
        }

        // parent process
        else {
            close(ssock);
        }
    }

}

int NETer::create_socket(string service)
{
    struct sockaddr_in sin; /* internet endpoint address */
    int socket_fd; /* socket descriptor and socket type */
    
    bzero((char *)&sin, sizeof(sin)); 
    sin.sin_family = AF_INET; 
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons((u_short)atoi(service.c_str()));

    // create socket
    socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        cout << "Error: cannot create socket !" << "\n";
        return -1;
    }

    int option = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int)) < 0) {
        cout << "Error: cannot set socket option !" << "\n";
        return -1;
    }

    // bind socket
    if(bind(socket_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        cout << "Error: cannot bind socket !" << "\n";
        return -1;
    }

    // listen on socket
    if(listen(socket_fd, 10) < 0) {
        cout << "Error: cannot listen on socket !" << "\n";
        return -1;
    }

    return socket_fd;
}

Shell* NETer::init_shell(
    string server_type, 
    int socket_fd,

    /**********************************************************************************************/
    /*                           server 2 (single process tcp server)                             */
    /**********************************************************************************************/
    map<int, pair<string, int> >* client_ip_port_table,
    map<int, string>* client_name_table,
    map<int, int>* client_id_table,
    map<pair<int, int>, int>* client_user_pipe_table,
    map<int, map<string, string> >* client_env_table,

    /**********************************************************************************************/
    /*                           server 3 (multi-process tcp server)                              */
    /**********************************************************************************************/
    ShM* client_shmer
)
{
    Shell* shell_env = new Shell();
    shell_env->activate(
        server_type, 
        socket_fd,
        client_ip_port_table,
        client_name_table,
        client_id_table,
        client_user_pipe_table,
        client_env_table,
        client_shmer
    );
    return shell_env;
}

void NETer::sigchld_handler(int signo)
{
    wait(nullptr);
}

void NETer::sigint_handler(int signo)
{
    /*
        this singal handler is used in server 3 multi-process (concurrent)
        tcp server with shared memory and fifo. when ctrl+c is pressed,
        server needs to clear shared memory and remove all fifo
    */

    // remove shared memory
    shmer.detach();
    shmer.remove();

    // remove fifo
    string path = "user_pipe";
    for (const auto & entry : filesystem::directory_iterator(path)) {
        unlink(entry.path().c_str());
    }
        

    exit(0);
}

void NETer::sigusr1_handler(int signo)
{
    /*
        this singal handler is used in server 3 multi-process (concurrent)
        tcp server with shared memory and fifo. when process receives this signal,
        it has to print the message stored in shared memory.
    */
    cout << shmer.get_message();
}

void NETer::sigusr2_handler(int signo)
{
    /*
        this singal handler is used in server 3 multi-process (concurrent)
        tcp server with shared memory and fifo. when process receives this signal,
        it has to open the fifo which another process creates.
    */
    shmer.set_user_pipe(-1, -1, "read");
}