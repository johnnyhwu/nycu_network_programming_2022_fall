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
        
		enum { max_length =  1000000};
        char data_[max_length];

        session(
			tcp::socket socket, 
			tcp::endpoint endpoint,
			int session_idx,
			string target_host,
			string target_port,
			string target_file
		) : socket_(std::move(socket)), 
			endpoint_(endpoint),
			session_idx_(session_idx),
			target_host_(target_host),
			target_port_(target_port),
			target_file_(target_file),
			file_content_idx_(0)
        {
            cerr << "[info, console.cgi] session is created" << endl;
        }

        void start()
        {
            cerr << "[info, console.cgi] session starts" << endl;

			// read file content
			read_file();
			cerr << "[info, console.cgi] read file: " << target_file_ << " (#lines: " << file_content_.size() << ") " << endl;

            connect_to_server();
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
};

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
				<title>NP Project 3 Console</title>
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
	string target_host;
	string target_port;
	string target_file;
	vector<string> tmp;
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
		tcp::resolver resolver(io_context);
		tcp::resolver::query query(target_host, target_port);
		tcp::resolver::iterator iter = resolver.resolve(query);
		tcp::endpoint endpoint = iter->endpoint();
		tcp::socket socket(io_context);
		make_shared<session>(
			move(socket), 
			endpoint,
			i+1,
			target_host, 
			target_port, 
			target_file
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