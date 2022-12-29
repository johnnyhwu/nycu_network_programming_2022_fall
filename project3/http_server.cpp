#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>

#include <string>
#include <vector>
#include <map>
#include <sys/wait.h>
#include <unistd.h>

using boost::asio::ip::tcp;
using namespace std;

void sigchld_handler(int signo)
{
    wait(nullptr);
}

class session : public std::enable_shared_from_this<session>
{
    public:
        tcp::socket socket_;
        enum { max_length = 1024 };
        char data_[max_length];

        session(tcp::socket socket) : socket_(std::move(socket))
        {
            cout << "[info, http_server] session is created" << endl;
            cout << "[info, http_server] remote address: " << socket_.remote_endpoint().address().to_string() << endl;
            cout << "[info, http_server] remote port: " << to_string(socket_.remote_endpoint().port()) << endl;
        }

        void start()
        {
            cout << "[info, http_server] session starts" << endl;
            parse_request();
        }

        void parse_request()
        {
            auto self(shared_from_this());
            socket_.async_read_some(
                boost::asio::buffer(data_, max_length),
                [this, self](boost::system::error_code ec, std::size_t length)
                {
                    cout << "[info, http_server] receive request:" << endl;
                    string request_header = data_;
                    cout << request_header << endl;
                    
                    if(request_header.length() > 20) {

                        cout << "[info, http_server] parse request" << endl;

                        map<string, string> env_table;

                        // split request header into sentences
                        vector<string> sentence_vec = split_string(request_header, "\r\n");
                        string program_path;

                        // save environment variable
                        env_table["REQUEST_METHOD"] = split_string(sentence_vec[0], " ")[0];
                        if(env_table["REQUEST_METHOD"] != "GET") {
                            cout << "[info, http_server] invalid request method" << endl;
                            return;
                        }

                        env_table["REQUEST_URI"] = split_string(sentence_vec[0], " ")[1];
                        if(env_table["REQUEST_URI"].find("?") != string::npos) {
                            env_table["QUERY_STRING"] = env_table["REQUEST_URI"].substr(env_table["REQUEST_URI"].find("?") + 1);
                            program_path = env_table["REQUEST_URI"].substr(0, env_table["REQUEST_URI"].find("?"));
                        } else {
                            env_table["QUERY_STRING"] = "";
                            program_path = env_table["REQUEST_URI"];
                        }
                        env_table["SERVER_PROTOCOL"] = split_string(sentence_vec[0], " ")[2];
                        env_table["HTTP_HOST"] = split_string(sentence_vec[1], " ")[1];
                        env_table["SERVER_ADDR"] = socket_.local_endpoint().address().to_string();
                        env_table["SERVER_PORT"] = to_string(socket_.local_endpoint().port());
                        env_table["REMOTE_ADDR"] = socket_.remote_endpoint().address().to_string();
                        env_table["REMOTE_PORT"] = to_string(socket_.remote_endpoint().port());
                        program_path = program_path.substr(1);

                        // show request header info
                        cout << "[info, http_server] request info:" << endl;
                        for(map<string, string>::iterator iter=env_table.begin(); iter!=env_table.end(); iter++) {
                            cout << iter->first << " : " << iter->second << endl;
                        }

                        if(program_path == "panel.cgi" || program_path == "console.cgi" || program_path == "printenv.cgi" || program_path == "hello.cgi" || program_path == "welcome.cgi") {
                            process_request(env_table, program_path);
                        } else {
                            cout << "[info, http_server] invalid program path: " << program_path << endl;
                        }
                        
                    } else {
                        cout << "[info, http_server] ignore request" << endl;
                    }
                }
            );
        }

        void process_request(map<string, string> env_table, string program_path)
        {
            cout << "[info, http_server] process request" << endl;

            auto self(shared_from_this());
            
            string response = "HTTP/1.0 200 OK\r\n";
            strcpy(data_, response.c_str());
            
            boost::asio::async_write(
                socket_, 
                boost::asio::buffer(data_, strlen(data_)),
                [this, self, env_table, program_path](boost::system::error_code ec, std::size_t /*length*/)
                {
                    if (!ec) {
                        // fork new process
                        pid_t pid = fork();

                        if (pid == 0) {
                            // set environment
                            set_env(env_table);

                            // duplicate socket fd to process's stdin/stdout
                            dup2(socket_.native_handle(), 0);
                            dup2(socket_.native_handle(), 1);

                            // exec program
                            char* args[100];
                            args[0] = NULL;
                            execv(program_path.c_str(), args);
                        }
                        else {
                            socket_.close();
                        }
                    }
                }
            );
        }

        vector<string> split_string(string text, string key)
        {
            size_t pos = 0;
            string part;
            vector<string> result;

            while ((pos = text.find(key)) != string::npos) {
                part = text.substr(0, pos);
                result.push_back(part);
                text.erase(0, pos + key.length());
            }

            if(text.length() > 0) {
                result.push_back(text);
            }

            return result;
        }

        void set_env(map<string, string> env_table) {
            // clear environment variable first
            clearenv();

            // set environment variable
            for (map<string, string>::iterator iter=env_table.begin(); iter!=env_table.end(); iter++) {
                setenv(iter->first.c_str(), iter->second.c_str(), 1);
            }
        }
};

class server
{
    public:
        tcp::acceptor acceptor_;

        server(boost::asio::io_context& io_context, short port) : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
        {
            cout << "[info, http_server] server is created" << endl;
            do_accept();
        }
        
        void do_accept()
        {
            acceptor_.async_accept(
                [this](boost::system::error_code ec, tcp::socket socket)
                {
                    if (!ec) {
                        cout << "-------------------------------------" << endl;
                        cout << "[info, http_server] connection is accepted" << endl;
                        std::make_shared<session>(std::move(socket))->start();
                    }

                    do_accept();
                }
            );
        }
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2) {
      std::cerr << "[error, http_server] async_tcp_echo_server <port>" << endl;;
      return 1;
    }

    signal(SIGCHLD, sigchld_handler);

    boost::asio::io_context io_context;

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "[error, http_server] " << e.what() << endl;;
  }

  return 0;
}