#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <regex>

using boost::asio::ip::tcp;
using namespace std;


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


class session : public std::enable_shared_from_this<session>
{
    public:
        tcp::socket socket_;
		tcp::endpoint endpoint_;
		int session_idx_;
		string target_host_;
		string target_port_;
		string target_file_;
		vector<string> file_content_;
		int file_content_idx_;

		// using proxy server
		bool socks_enable_;
		tcp::endpoint socks_endpoint_;
		int read_from_socks_count_;

        
		enum { max_length =  1000000};
        char data_[max_length];

        session(
			tcp::socket socket, 
			tcp::endpoint endpoint,
			int session_idx,
			string target_host,
			string target_port,
			string target_file,

			// using proxy server
			bool socks_enable,
			tcp::endpoint socks_endpoint

		) : socket_(std::move(socket)), 
			endpoint_(endpoint),
			session_idx_(session_idx),
			target_host_(target_host),
			target_port_(target_port),
			target_file_(target_file),
			file_content_idx_(0),

			// using proxy server
			socks_enable_(socks_enable),
			socks_endpoint_(socks_endpoint),
			read_from_socks_count_(0)
        {
            cerr << "[info, console.cgi] session is created" << endl;
        }

        void start()
        {
            cerr << "[info, console.cgi] session starts" << endl;

			// read file content
			read_file();
			cerr << "[info, console.cgi] read file: " << target_file_ << " (#lines: " << file_content_.size() << ") " << endl;

			if(socks_enable_) {
				connect_to_socks();
			} else {
				connect_to_server();
			}
        }

        void connect_to_server()
        {
            auto self(shared_from_this());
            socket_.async_connect(
                endpoint_,
                [this, self](boost::system::error_code ec)
				{
					if (!ec) {
						cerr << "[info, console.cgi] connects to server" << endl;
						
						// write to client: update session's address and port
						cout << "<script>document.querySelector('body > div:nth-child(" << to_string(session_idx_) << ")').children[1].children[0].innerHTML += '" << target_host_ << "'</script>" << flush;
						cout << "<script>document.querySelector('body > div:nth-child(" << to_string(session_idx_) << ")').children[1].children[1].innerHTML += '" << target_port_ << "'</script>" << flush;

						// write to client: clean console
						cout << "<script>document.querySelector('body > div:nth-child(" << to_string(session_idx_) << ")').children[2].innerHTML = ''</script>" << flush;
						
						// read from host
						read_from_host();
					}
				}
            );
        }

		void read_from_host()
		{
			auto self(shared_from_this());
			clear_buffer();
			socket_.async_read_some(boost::asio::buffer(data_, max_length),
				[this, self](boost::system::error_code ec, std::size_t length)
				{
					if (!ec) {
						string host_output = data_;
						cerr << "[info, console.cgi] host output: " << endl;
						cerr << host_output << endl;

						// escape special character
						host_output = escape_character(host_output);

						// write to client: write the output of host to client
						cout << "<script>document.querySelector('body > div:nth-child(" << to_string(session_idx_) << ")').children[2].innerHTML += `" << host_output << "`</script>" << flush;

						// find if '%' in host_output
						if(host_output.find("%") != std::string::npos) {
							write_to_host();
						}
						else{
							clear_buffer();
							read_from_host();
						}
					}
				}
			);
		}

		void write_to_host()
		{
			auto self(shared_from_this());
			string cmd = file_content_[file_content_idx_];
			file_content_idx_++;

			// write to client: write the input of host to client
			cout << "<script>document.querySelector('body > div:nth-child(" << to_string(session_idx_) << ")').children[2].innerHTML += `<pre class=\"input-cmd\">" << cmd << "</pre>`</script>" << flush;

			boost::asio::async_write(socket_, boost::asio::buffer(cmd.c_str(), cmd.length()),
				[this, self](boost::system::error_code ec, std::size_t /*length*/)
				{
					if (!ec) {
						read_from_host();
					}
				}
			);
		}

		void clear_buffer()
		{
			for(size_t i=0; i<max_length; i++) {
				data_[i] = '\0';
			}
		}

		void read_file()
		{
			string text;
			string filename = "test_case/" + target_file_;
			ifstream fs(filename);

			while (getline(fs, text)) {
				file_content_.push_back(text + "\n");
			}

			fs.close(); 
		}

		string escape_character(string text)
		{
			text = regex_replace(text, regex(R"(&)"), "&amp;");
			text = regex_replace(text, regex(R"(>)"), "&gt;");
			text = regex_replace(text, regex(R"(<)"), "&lt;");
			text = regex_replace(text, regex(R"(")"), "&quot;");
			text = regex_replace(text, regex(R"(')"), "&apos;");
			return text;
		}


		/*************************************************************************************************************/

		void write_to_socks() {
			auto self(shared_from_this());

			unsigned short host_port = endpoint_.port();
			unsigned short host_port_low = host_port % 256;
			unsigned short host_port_high = host_port >> 8;

			string host_ip = endpoint_.address().to_string();
			vector<string> host_ip_nums = split_string(host_ip, ".");
		
			unsigned char msg[9] = {4, 1, host_port_high, host_port_low, stoi(host_ip_nums[0]), stoi(host_ip_nums[1]), stoi(host_ip_nums[2]), stoi(host_ip_nums[3]), 0};
			clear_buffer();
			memcpy(data_, msg, 9);

			socket_.async_write_some(
				boost::asio::buffer(data_, 9),
				[this, self](boost::system::error_code ec, size_t length) {
					if(!ec) {
						// read from host
						read_from_socks();
					}
				}
			);
		}

		void read_from_socks(){
			auto self(shared_from_this());
			socket_.async_read_some(
				boost::asio::buffer(data_, max_length),
				[this, self](boost::system::error_code ec, size_t length) {
					if (!ec) {
						unsigned short status_code = data_[1];

						// socks server connects to host successfully
						if(status_code == 90) {
							// write to client: update session's address and port
							cout << "<script>document.querySelector('body > div:nth-child(" << to_string(session_idx_) << ")').children[1].children[0].innerHTML += '" << target_host_ << "'</script>" << flush;
							cout << "<script>document.querySelector('body > div:nth-child(" << to_string(session_idx_) << ")').children[1].children[1].innerHTML += '" << target_port_ << "'</script>" << flush;

							// write to client: clean console
							cout << "<script>document.querySelector('body > div:nth-child(" << to_string(session_idx_) << ")').children[2].innerHTML = ''</script>" << flush;

							// print host's reply to client's console
							if(length > 8) {
								string host_output;
								for(int i=8; i<length; i++) {
									string tmp(1, data_[i]);
									host_output += tmp;
								}

								// escape special character
								host_output = escape_character(host_output);

								// write to client: write the output of host to client
								cout << "<script>document.querySelector('body > div:nth-child(" << to_string(session_idx_) << ")').children[2].innerHTML += `" << host_output << "`</script>" << flush;

								// find if '%' in host_output
								if(host_output.find("%") != std::string::npos) {
									write_to_host();
								}
								else{
									clear_buffer();
									read_from_host();
								}
							} else {
								clear_buffer();
								read_from_host();
							}
						}

						// socks server cannot connect to host
						else {
							write_to_socks();
						}
					}
				}
			);
		}

		void connect_to_socks()
        {
			socket_.connect(socks_endpoint_);
			write_to_socks();
        }
};

int main(int argc, char* argv[])
{
  try
  {
    // response header
    cout << "Content-type: text/html\r\n\r\n";

    // html content
    string html_content = R"(
		<!DOCTYPE html>
		<html lang="en">
			<head>
				<title>NP Project 4 Console</title>
				<link
				href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
				rel="stylesheet"
				/>
				<link
				rel="icon"
				type="image/png"
				href="https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png"
				/>
				<style>
				* {
				font-family: 'Source Code Pro', monospace;
				}
				</style>
			</head>
			<body>
				<div class="console">
					<div class="title">Session 1</div>
					<div class="remote">
						<div class="info">ADDR: </div>
						<div class="info">PORT: </div>
					</div>
					<pre class="content">

					</pre>
				</div>

				<div class="console">
					<div class="title">Session 2</div>
					<div class="remote">
						<div class="info">ADDR: </div>
						<div class="info">PORT: </div>
					</div>
					<pre class="content">

					</pre>
				</div>

				<div class="console">
					<div class="title">Session 3</div>
					<div class="remote">
						<div class="info">ADDR: </div>
						<div class="info">PORT: </div>
					</div>
					<pre class="content">

					</pre>
				</div>

				<div class="console">
					<div class="title">Session 4</div>
					<div class="remote">
						<div class="info">ADDR: </div>
						<div class="info">PORT: </div>
					</div>
					<pre class="content">

					</pre>
				</div>

				<div class="console">
					<div class="title">Session 5</div>
					<div class="remote">
						<div class="info">ADDR: </div>
						<div class="info">PORT: </div>
					</div>
					<pre class="content">

					</pre>
				</div>
			</body>
      	</html>

		<style>
			body {
				margin: 0;
				padding: 0;
				box-sizing: border-box;

				display: flex;
				flex-direction: row;
				justify-content: flex-start;
				align-items: flex-start;

				background-color: rgb(30, 30, 30);
			}

			div.console {
				height: 100vh;

				border-color: white;
				border-style: solid;
				border-width: 2px;
				border-radius: 20px;

				display: flex;
				flex-direction: column;
				justify-content: flex-start;
				align-items: center;
			}

				div.title {
					width: 100%;
					box-sizing: border-box;
					padding: 10px 0px;

					color: white;
					font-size: 1.5rem;
					
					display: flex;
					flex-direction: row;
					justify-content: center;
					align-items: center;

					border-bottom-color: white;
					border-bottom-width: 2px;
					border-bottom-style: solid;
				}

				div.remote {
					width: 100%;
					box-sizing: border-box;
					padding: 5px 10px;

					color: white;
					font-size: 1.2rem;
					
					display: flex;
					flex-direction: column;
					justify-content: center;
					align-items: flex-start;

					border-bottom-color: white;
					border-bottom-width: 2px;
					border-bottom-style: solid;
				}

				pre.content {
					width: 100%;
					height: 100%;
					box-sizing: border-box;
					padding: 5px 10px;

					color: white;
					font-size: 1.0rem;

					overflow-y: overlay;
				}

					pre.input-cmd {
						display: inline;
    					color: greenyellow;
					}
		</style>
    )";
    cout << html_content;

    // create io context
    boost::asio::io_context io_context;

    // parse query string
	string raw_query = getenv("QUERY_STRING");
    vector<string> query = split_string(raw_query, "&");
	vector<string> tmp;

	// target host info
	string target_host;
	string target_port;
	string target_file;

	// socks server info
	string socks_ip;
	string socks_port;
	bool socks_enable = false;
	
	tmp = split_string(query.back(), "=");
	if(tmp.size() == 2) {
		socks_port = tmp[1];
	}
	
	tmp = split_string(query[query.size()-2], "=");
	if(tmp.size() == 2) {
		socks_ip = tmp[1];
	}

	if(socks_ip != "" && socks_port != "") {
		socks_enable = true;
	}
	
    for(size_t i=0; i<5; i++) {
		// host info
		tmp = split_string(query[3*i + 0], "=");
		if(tmp.size() == 1) {
			break;
		}
		target_host = tmp[1];

		// port info
		tmp = split_string(query[3*i + 1], "=");
		target_port = tmp[1];

		// file info
		tmp = split_string(query[3*i + 2], "=");
		target_file = tmp[1];

		cerr << "[info, console.cgi] target host: " << target_host << endl;
		cerr << "[info, console.cgi] target port: " << target_port << endl;
		cerr << "[info, console.cgi] target file: " << target_file << endl;

		// build new session
		tcp::resolver host_resolver(io_context);
		tcp::resolver::query host_query(target_host, target_port);
		tcp::resolver::iterator host_iter = host_resolver.resolve(host_query);
		tcp::endpoint host_endpoint = host_iter->endpoint();
		

		tcp::resolver socks_resolver(io_context);
		tcp::resolver::query socks_query(socks_ip, socks_port);
		tcp::resolver::iterator socks_iter = socks_resolver.resolve(socks_query);
		tcp::endpoint socks_endpoint = socks_iter->endpoint();

		tcp::socket socket(io_context);

		make_shared<session>(
			move(socket), 
			host_endpoint,
			i+1,
			target_host, 
			target_port, 
			target_file,

			// using proxy server
			socks_enable,
			socks_endpoint
		)->start();
    }

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "[error, console.cgi] " << e.what() << "\n";
  }

  return 0;
}