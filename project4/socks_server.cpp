#include <cstdlib>
#include <memory>
#include <utility>
#include <boost/asio.hpp>

#include <sys/wait.h>
#include <unistd.h>

#include "src/client.hpp"

using boost::asio::ip::tcp;
using namespace std;

void sigchld_handler(int signo)
{
    wait(nullptr);
}

class server
{
    public:
        tcp::acceptor acceptor_;
        boost::asio::io_context& io_context_;

        server(boost::asio::io_context& io_context, short port) : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), io_context_(io_context)
        {
            // cout << "[info, socks_server] server is created" << endl;
            do_accept();
        }
        
        void do_accept()
        {
            acceptor_.async_accept(
                [this](boost::system::error_code ec, tcp::socket socket)
                {
                    if (!ec) {
                        io_context_.notify_fork(boost::asio::io_context::fork_prepare);

                        pid_t pid = fork();
                        
                        // child porcess
                        if(pid == 0) {
                            io_context_.notify_fork(boost::asio::io_context::fork_child);
                            acceptor_.close();
                            std::make_shared<client_session>(std::move(socket), io_context_)->start();
                        }
                        
                        // parent process
                        else {
                            io_context_.notify_fork(boost::asio::io_context::fork_parent);
                            socket.close();
                            do_accept();
                        }
                        
                    } else {
                        // cout  << "-------------------------------------" << endl;
                        cerr << "[error, socks_server] connection cannot be accepted" << endl;
                    }
                }
            );
        }
};

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 2) {
            cerr << "[error, socks_server] async_tcp_echo_server <port>" << endl;;
            return 1;
        }

        signal(SIGCHLD, sigchld_handler);

        boost::asio::io_context io_context;

        server s(io_context, std::atoi(argv[1]));

        io_context.run();
    }
    catch (std::exception& e)
    {
        cerr << "[error, socks_server] " << e.what() << endl;;
    }

    return 0;
}















// #include <boost/asio.hpp>
// #include <cstdlib>
// #include <iostream>
// #include <memory>
// #include <utility>
// #include <stdio.h>
// #include <iomanip>
// #include <sstream>
// #include <string>
// #include <string.h>
// #include "router.hpp"

// using boost::asio::ip::tcp;
// using namespace std;

// class server {
//    public:
//     tcp::acceptor acceptor_;
//     boost::asio::io_context& io_context;

//     server(boost::asio::io_context& io_context, short port)
//         : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), io_context(io_context) {
//         // cout << acceptor_.local_endpoint().port() << endl;
//         do_accept();
//     }

//     void do_accept() {
//         acceptor_.async_accept(
//             [this](boost::system::error_code ec, tcp::socket socket) {
//                 if (!ec) {
//                     io_context.notify_fork(boost::asio::io_context::fork_prepare);
//                     if (fork() == 0) { // child
//                         io_context.notify_fork(boost::asio::io_context::fork_child);
//                         acceptor_.close();
//                         make_shared<router>(move(socket), io_context)->do_parse();
//                     }
//                     else{ // parent
//                         io_context.notify_fork(boost::asio::io_context::fork_parent);
//                         socket.close();
//                         do_accept();
//                     }
//                 }
//             }
//         );
//     }
// };

// int main(int argc, char* argv[]) {
//     try {
//         if (argc != 2) {
//             cerr << "Usage: async_tcp_echo_server <port>\n";
//             return 1;
//         }

//         boost::asio::io_context io_context;
//         server s(io_context, atoi(argv[1]));
//         cout << "starting !" << endl;
//         io_context.run();

//     } catch (exception& e) {
//         cerr << "Exception: " << e.what() << "\n";
//     }

//     return 0;
// }