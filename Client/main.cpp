/*
    Ainoras Zukauskas 4gr
*/

#ifdef _WIN32
//-------------------------------------------
#include <winsock2.h>
#include <errno.h>
#include <ws2tcpip.h>
//-------------------------------------------
#endif // _WIN32

#ifdef linux
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#endif // linux

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <string>
#include <iostream>
#include <vector>

#define BUFFLEN 1024

using namespace std;

enum class RecieveStatus {
    OK,
    ERR
};

enum class SendStatus
{
    OK,
    ERR
};

SendStatus send_to_socket(int s_socket, string content){
    int write_len;
    int content_len;
	do{
	    content_len = content.length() + 1; //+1 for '\0' which is excluded from .length()
		write_len = send(s_socket, content.c_str(), content_len, 0);
		if (write_len <= 0) {
			return SendStatus::ERR;
		}

		if (content_len > write_len) {
			 content =  content.substr(write_len);
		}
	} while (content_len != write_len);

    return SendStatus::OK;
}

RecieveStatus recieve_from_socket(int s_socket, string &outContent) {
    outContent = "";

	char buffer[BUFFLEN] = { 0 };
	int read_len;
    do{
		read_len = recv(s_socket, buffer, BUFFLEN, 0);
		if (read_len <= 0) {
			return RecieveStatus::ERR;
		}

        // '\0' should not be put into string
        if(buffer[read_len - 1] == '\0'){
            outContent += std::string(buffer, read_len - 1);
        }
        else{
            outContent += std::string(buffer, read_len);
        }
	} while (buffer[read_len - 1] != '\0');

	return RecieveStatus::OK;
}

void cleanup(int s_socket){
    #ifdef __MINGW32__
        closesocket(s_socket);
        WSACleanup();
    #endif // _WIN32
    #ifdef linux
        close(s_socket);
    #endif // linux
}

void split(const string &str, vector<string> &split_strings, char delim){
    size_t current, previous = 0;
    current = str.find(delim);
    while (current != string::npos) {
        split_strings.push_back(str.substr(previous, current - previous));
        previous = current + 1;
        current = str.find(delim, previous);
    }
    split_strings.push_back(str.substr(previous, current - previous));
}

void clear_screen()
{
    #ifdef __MINGW32__
        std::system("cls");
    #else
        // Assume POSIX
        std::system ("clear");
    #endif
}

void display_game(const vector<string> &tokens){
    cout<<"| |1|2|3|";
    string column_names = "ABC";

    int token_count = 0;
    for(int token_index = 2; token_index < tokens.size(); token_index++){
        if(token_count % 3 == 0){
            cout<<endl<<"|"<<column_names[token_count/3]<<"|";
        }

        if(tokens[token_index] == "Empty"){
            cout<<" |";
        }
        else{
            cout<<tokens[token_index]<<"|";
        }

        token_count++;
    }

    cout<<endl<<endl;
}

int main(int argc, char *argv[]){
    if (argc != 3){
        fprintf(stderr,"USAGE: %s <ip> <port>\n",argv[0]);
        exit(1);
    }

    #ifdef __MINGW32__
    //-------------------------------------------
    //Kad veiktu su Windows

    WSADATA wsaData;   // if this doesn't work
    //WSAData wsaData; // then try this instead

    // MAKEWORD(1,1) for Winsock 1.1, MAKEWORD(2,0) for Winsock 2.0:

    if (WSAStartup(MAKEWORD(1,1), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        exit(1);
    }
    //-------------------------------------------
    #endif // _WIN32

    unsigned int port = atoi(argv[2]); //atoi() ascii to integer. Vietoj inet_ntoa()

    if ((port < 1) || (port > 65535)){
        fprintf(stderr, "ERROR #1: invalid port specified.\n");
        exit(1);
    }

    //Create server socket
    int s_socket;
    if ((s_socket = socket(AF_INET, SOCK_STREAM,0))< 0){ //socket is a way to speak to other programs using standard Unix file descriptors. socket() returns a new socket descriptor
        fprintf(stderr, "ERROR #2: cannot create socket.\n");
        exit(1);
    }

    //Create server struct
    sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET; //AF_INET (IPv4) or AF_INET6 (IPv6)
    servaddr.sin_port = htons(port); //htons() host to network long

    //Convert and store pass ip to serveradrr
    #ifdef linux
    if (inet_aton(argv[1], &servaddr.sin_addr) <= 0 ) {
        fprintf(stderr,"ERROR #3: Invalid remote IP address.\n");
        exit(1);
    }
    #endif // linux
    #ifdef __MINGW32__
    if ((servaddr.sin_addr.s_addr = inet_addr(argv[1])) == -1 ) { //inet_addr() converting from a dots-and-numbers string into a in_addr_t
        fprintf(stderr,"ERROR #3: Invalid remote IP address.\n");
        exit(1);
    }
    #endif // _WIN32

    if (connect(s_socket, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0){ //Once the socket is connect()ed, you're free to send() and recv() data on it
        fprintf(stderr,"ERROR #4: error in connect().\n");
        exit(1);
    }

    #ifdef __MINGW32__
        u_long nonblocking_enabled = TRUE;
        ioctlsocket(0, FIONBIO, &nonblocking_enabled); //ioctlsocket() controls the I/O mode of a socket.
    #endif // _WIN32

    do{
        string content;
        auto recvStatus = recieve_from_socket(s_socket, content);

        if(recvStatus == RecieveStatus::ERR){
            break;
        }

        clear_screen();

        vector<string> tokens;
        split(content, tokens, '|');
        if(tokens.size() < 1){
            break;
        }

        if(tokens[0] == "MENU"){
            cout<<"Games:"<<endl;
            for(unsigned int token_index = 1; token_index < tokens.size(); token_index++){
                cout << tokens[token_index] << endl;
            }

            cout<<endl<<endl;
            cout<<"Commands:"<<endl;
            cout<<"CREATE|<NAME>"<<endl;
            cout<<"JOIN|<NAME>"<<endl;

            getline(cin, content);
            auto sendStatus = send_to_socket(s_socket, content);
            if(sendStatus == SendStatus::ERR){
                break;
            }
        }
        else if(tokens[0] == "GAME"){
            if(tokens.size() != 11){
                cout<<"Client incompatible"<<endl;
                break;
            }

            if(tokens[1] == "BLOCKED" ){
                cout<<"Waiting for opponent..."<<endl<<endl;
                display_game(tokens);
                continue;
            }

            if(tokens[1] == "TURN"){
                cout<<"It is your turn!"<<endl<<endl;
                display_game(tokens);

                cout<<"Select field: EX: A1"<<endl;
                getline(cin, content);
                auto sendStatus = send_to_socket(s_socket, content);
                if(sendStatus == SendStatus::ERR){
                    break;
                }
            }
        }
        else if(tokens[0] == "WIN" || tokens[0] == "LOSS" || tokens[0] == "DRAW"){
            if(tokens.size() != 2){
                cout<<"Client incompatible"<<endl;
                break;
            }

            if(tokens[0] == "WIN"){
                cout << "Congratulations, You won the game!" << endl;
            }
            else if(tokens[0] == "LOSS"){
                cout << "Better luck next time!" << endl;
            }
            else{
                cout << "It's a draw!" << endl;
            }

            cout << "This game took " << tokens[1] << " turns to complete." << endl;

            cout << "Enter any command to return to main menu." << endl;
            getline(cin, content);
            auto sendStatus = send_to_socket(s_socket, content);
            if(sendStatus == SendStatus::ERR){
                break;
            }
        }
    }
    while(true);

    cleanup(s_socket);

    return 0;
}
