#include "client.hpp"
#include "bind.hpp"

client_session::client_session(tcp::socket socket, boost::asio::io_context& io_context) : socket_(std::move(socket)), io_context_(io_context)
{
    // cout << "[info, socks_server (client session)] session is created" << endl;

    version = "";
    command = "";
    dest_port = "";
    dest_ip = "";
    domain = "";
    data_[0] = '\0';
}

void client_session::start()
{
    parse_request();
}

void client_session::parse_request()
{
    auto self(shared_from_this());
    socket_.async_read_some(
        boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
            if(!ec) {
                
                version = to_string(data_[0]);
                command = to_string(data_[1]);

                int dest_port_high = (int)data_[2];
                dest_port_high = dest_port_high << 8;
                int dest_port_low = (int)data_[3];
                dest_port = to_string(dest_port_high + dest_port_low);

                for(int i=4; i<8; i++) {
                    dest_ip += to_string(data_[i]);
                    if(i != 7) {
                        dest_ip += ".";
                    }
                }

                size_t null_pos_start = 0;
                size_t null_pos_end = 0;
                for(size_t i=8; i<length; i++) {
                    if(data_[i] == 0x00) {
                        if (null_pos_start == 0) {
                            null_pos_start = i;
                        } else {
                            null_pos_end = i;
                        }
                    }
                }

                if(null_pos_start != 0 && null_pos_end != 0) {
                    for(size_t i=null_pos_start+1; i<null_pos_end; i++) {
                        domain += data_[i];
                    }
                }

                exec_request(command, dest_port, dest_ip, domain);
            }
        }
    );
}

void client_session::exec_request(string command, string dest_port, string dest_ip, string domain)
{
    // CONNECT
    if(command == "1") {
		// create new socket for host
		tcp::resolver resolver(io_context_);
        if(domain != "") {
            dest_ip = domain;
        }
		tcp::resolver::query query(dest_ip, dest_port);
		tcp::resolver::iterator iter = resolver.resolve(query);
		tcp::endpoint endpoint = iter->endpoint();
		tcp::socket host_socket(io_context_);

        cout << "<S_IP>: " << socket_.remote_endpoint().address().to_string() << endl;
        cout << "<S_PORT>: " << socket_.remote_endpoint().port() << endl;
        cout << "<D_IP>: " << endpoint.address().to_string() << endl;
        cout << "<D_PORT>: " << endpoint.port() << endl;
        cout << "<Command>: " << "CONNECT" << endl;

        // validate this request
        if(firewall_validate('c', endpoint.address().to_string())) {
            cout << "<Reply>: " << "Accept" << endl;
            cout << endl;
            make_shared<host_session>(move(socket_), move(host_socket), endpoint)->start();
            io_context_.run();
        } else {
            cout << "<Reply>: " << "Reject" << endl;
            reject_client();
        }
    }

    // BIND
    else {
        cout << "<S_IP>: " << socket_.remote_endpoint().address().to_string() << endl;
        cout << "<S_PORT>: " << socket_.remote_endpoint().port() << endl;
        cout << "<D_IP>: " << dest_ip << endl;
        cout << "<D_PORT>: " << dest_port << endl;
        cout << "<Command>: " << "BIND" << endl;

        // validate this request
        if(firewall_validate('b', dest_ip)) {
            cout << "<Reply>: " << "Accept" << endl;
            cout << endl;
            make_shared<bind_session>(move(socket_), io_context_)->start();
        } else {
            cout << "<Reply>: " << "Reject" << endl;
            reject_client();
        }
    }

    cout << endl;
}

void client_session::reject_client()
{
    auto self(shared_from_this());
    unsigned char msg[8] = {0, 91, 0, 0, 0, 0, 0, 0};
    memcpy(data_, msg, 8);
    boost::asio::async_write(
        socket_, boost::asio::buffer(data_, 8),
        [this, self](boost::system::error_code ec, size_t length) {
            if (!ec) {
                socket_.close();
            }
        }
    );
}

vector<string> client_session::split_string(string text, string key)
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

bool client_session::firewall_validate(char ops, string dest_ip)
{
    // read socks.conf
    ifstream f("socks.conf");
    string text;
    vector<string> rule;
    while (getline(f, text)) {
        rule.push_back(text);
    }
    f.close();

    // validate dest ip
    vector<string> dest_ip_ = split_string(dest_ip, ".");
    if(dest_ip_.size() != 4) {
        return false;
    }

    for(size_t i=0; i<rule.size(); i++) {
        if(rule[i][7] != ops) {
            continue;
        }

        vector<bool> pass(4, false);

        string ip = rule[i].substr(9);
        vector<string> ips = split_string(ip, ".");
        for(size_t i=0; i<ips.size(); i++) {
            if(ips[i] == dest_ip_[i] || ips[i] == "*") {
                pass[i] = true;
            }
        }

        if(pass[0] && pass[1] && pass[2] && pass[3]) {
            return true;
        }
    }
    
    return false;
}