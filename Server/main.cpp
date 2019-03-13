/*
    Ainoras Zukauskas 4gr
*/

#ifdef _WIN32
#include <winsock2.h>
#include <errno.h>
#include <ws2tcpip.h>
#endif // _WIN32

#ifdef linux
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#endif // linux


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <vector>
#include <string>
#include <iostream>
#include <string>
#include <algorithm>
#include <iterator>
#include <memory>
#include <map>

#define BUFFLEN 1024
#define MAXCLIENTS 20
#define LIVES 5
#define MAXQUEUE 5

using namespace std;

enum class RecieveStatus {
    OK,
    PARTIAL,
    ERR
};

enum class SendStatus
{
    OK,
    ERR
};

enum class PlayerState {
	Missing,
	Menu,
	GameTurn,
	GameBlocked,
	Win,
	Loss,
	Draw
};

struct Game;
struct Player {
    int id;
	int c_socket;
	PlayerState pState = PlayerState::Missing;
	shared_ptr<Game> game = nullptr;

	string incomingBuffer = "";
	string outgoingBuffer = "";
};

enum class TicTacToe {
	Empty,
	X,
	O
};

struct Game {
	Player* player1;
	Player* player2;
	bool finished = false;
	bool turn_taken = false;
	string name = "";

	TicTacToe game_state[9] = { TicTacToe::Empty };
};

int find_empty_user(Player players[]) {
	for (int client_id = 0; client_id < MAXCLIENTS; client_id++) {
		if (players[client_id].pState == PlayerState::Missing) {
			return client_id;
		}
	}
	return -1;
}

void win_screen(Player &player);

void cleanup(Player &player) {
    cout << "Dropped connection with client " << player.id << endl;

	#ifdef _WIN32
		closesocket(player.c_socket);
	#endif // _WIN32
	#ifdef linux
		close(player.c_socket);
	#endif // linux

	PlayerState tempState = player.pState;
	player.pState = PlayerState::Missing;
	if(player.game == nullptr){
        return;
	}

    player.game->finished = true;
	if(player.game->player1->id == player.id){
        player.game->player1 = nullptr;

        if(tempState == PlayerState::GameTurn){
            win_screen(*(player.game->player2)); //Other player automatically wins
        }

	}
	else{
        player.game->player2 = nullptr;

        if(tempState == PlayerState::GameTurn){
            win_screen(*(player.game->player1)); //Other player automatically wins
        }
	}

    player.game = nullptr;
}

SendStatus send_to_socket(Player &player){
    int write_len;
    int content_len;
	do
	{
	    content_len = player.outgoingBuffer.length() + 1; //+1 for '\0' which is excluded from .length()
		write_len = send(player.c_socket, player.outgoingBuffer.c_str(), content_len, 0);
		if (write_len <= 0) {
            player.outgoingBuffer = "";
			cleanup(player);
			return SendStatus::ERR;
		}

		if (content_len > write_len) {
			 player.outgoingBuffer =  player.outgoingBuffer.substr(write_len);
		}
	} while (content_len != write_len);

	player.outgoingBuffer = "";

    return SendStatus::OK;
}

RecieveStatus recieve_from_socket(Player &player) {
	char buffer[BUFFLEN] = { 0 };

	int read_len = recv(player.c_socket, buffer, BUFFLEN, 0);
	if (read_len <= 0) {
		cleanup(player);
		return RecieveStatus::ERR;
	}

    // '\0' should not be put into string
    if(buffer[read_len - 1] == '\0'){
        player.incomingBuffer += std::string(buffer, read_len - 1);
    }
    else{
        player.incomingBuffer += std::string(buffer, read_len);
    }

	if (buffer[read_len - 1] != '\0') {
		return RecieveStatus::PARTIAL;
	}

	return RecieveStatus::OK;
}

//Sends available open games to player
void send_menu_information(Player &player, const vector<shared_ptr<Game>> &games){
    string content = "MENU";
    for(shared_ptr<Game> game : games){
        if(game->finished){
            continue;
        }

        if(game->player1 != nullptr && game->player2 != nullptr){
            continue;
        }

        content += "|" + game->name;
    }

    player.outgoingBuffer = content; //menu info
    send_to_socket(player);
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

//finds if game with name already exists
shared_ptr<Game> find_game(const vector<shared_ptr<Game>> &games, string name){
    for(shared_ptr<Game> game : games){
        if(game->name == name){
            return game;
        }
    }

    return nullptr;
}

//Converts game state to string for sending over socket
string game_to_string(shared_ptr<Game> game){
    string ret = "";
    for(int index = 0; index < 9; index++){
        switch(game->game_state[index]){
            case TicTacToe::X:
                ret += "|X";
                break;
            case TicTacToe::O:
                ret += "|O";
                break;
            default:
                ret += "|Empty";
                break;
        }
    }

    return string(ret);
}

//Sends game state information to specified player
void update_game_state(Player &player) {
    string content = "GAME";

    switch(player.pState){
        case PlayerState::GameBlocked:
            content += "|BLOCKED";
            break;
        case PlayerState::GameTurn:
            content += "|TURN";
            break;
        default:
            return;
    }

    content += game_to_string(player.game);

    player.outgoingBuffer = content; //game info
    send_to_socket(player);
}

//Creates new game room and puts player in that room
bool menu_create(Player &player, vector<shared_ptr<Game>> &games, string name) {
    if(find_game(games, name) != nullptr){
        return false;
    }

    cout << "Player " << player.id << " created game named: " << name << endl;

    player.pState = PlayerState::GameBlocked;

    player.game = make_shared<Game>();
    games.push_back(player.game);

    player.game->player1 = &player;
    player.game->name = name;

    update_game_state(player);

    return true;
}

//Puts player in existing room
bool menu_join(Player &player, vector<shared_ptr<Game>> &games, string name) {
    if((player.game = find_game(games, name)) == nullptr){
        return false;
    }

    if(player.game->player2 != nullptr){
        return false;
    }

    cout << "Player " << player.id << " joined game named: " << player.game->name << endl;

    player.game->player2 = &player;
    player.pState = PlayerState::GameTurn;
    player.game->player1->pState = PlayerState::GameBlocked;

    update_game_state(player);

    return true;
}

//Parses through menu commands and runs them
void menu_commands(Player &player, vector<shared_ptr<Game>> &games) {
    vector<string> split_strings;
    split(player.incomingBuffer, split_strings, '|');

    bool isSuccess = false;
    if(split_strings.size() == 2){
        //CREATE|NAME
        if(split_strings[0] == "CREATE"){
            isSuccess = menu_create(player, games, split_strings[1]);
        }
        //JOIN|NAME
        else if(split_strings[0] == "JOIN"){
            isSuccess = menu_join(player, games, split_strings[1]);
        }
    }

    if(isSuccess){
        return;
    }

    if(player.pState == PlayerState::Missing){
        return;
    }

    send_menu_information(player, games);
}

bool is_game_draw(const Player &player){
    for(TicTacToe field : player.game->game_state){
        if(field == TicTacToe::Empty){
            return false;
        }
    }

    return true;
}

bool is_game_won(const Player &player, TicTacToe mark){
    int winning_combinations[8][3] = {
        {0, 1, 2},
        {3, 4, 5},
        {6, 7, 8},
        {0, 3, 6},
        {1, 4, 7},
        {2, 5, 8},
        {0, 4, 8},
        {2, 6, 4}
    };

    for(int combination_index = 0; combination_index < 8; combination_index++){
        bool win = true;
        for(int field_index = 0; field_index < 3; field_index++){
            if(player.game->game_state[winning_combinations[combination_index][field_index]] != mark){
                win = false;
            }
        }

        if(win) return win;
    }

    return false;
}

//Counts total turns taken in the game
int count_game_turns(Player &player){
    int turn_count = 0;
    for(TicTacToe field : player.game->game_state){
        if(field != TicTacToe::Empty){
            turn_count++;
        }
    }

    return turn_count;
}

//Sends winning screen info to player
void win_screen(Player &player){
    cout << "Player " << player.id << " won game named: " << player.game->name << endl;

    int turn_count = count_game_turns(player);

    player.pState = PlayerState::Win;
    player.game->finished = true;
    player.game = nullptr;

    player.outgoingBuffer = "WIN|" + to_string(turn_count);
    send_to_socket(player);
}

//Sends losing screen info to player
void lose_screen(Player &player){
    cout << "Player " << player.id << " lost game named: " << player.game->name << endl;
    int turn_count = count_game_turns(player);

    player.pState = PlayerState::Loss;
    player.game->finished = true;
    player.game = nullptr;

    player.outgoingBuffer = "LOSS|" + to_string(turn_count);
    send_to_socket(player);
}

//Sends draw screen info to player
void draw_screen(Player &player){
    cout << "Player " << player.id << " drew game named: " << player.game->name << endl;

    player.pState = PlayerState::Draw;
    player.game->finished = true;
    player.game = nullptr;

    player.outgoingBuffer = "DRAW|9";
    send_to_socket(player);
}


//Process logic for making a valid move in the game
void game_make_move(Player &player, int move_index){
    if(player.game->turn_taken){
        update_game_state(player);
        return;
    }

    if(player.game->game_state[move_index] != TicTacToe::Empty){
        update_game_state(player);
        return;
    }

    TicTacToe mark;
    if(player.game->player2->id == player.id){
        mark = TicTacToe::X;
        player.game->player1->pState = PlayerState::GameTurn;
    }
    else{
        mark = TicTacToe::O;
        player.game->player2->pState = PlayerState::GameTurn;
    }

    player.game->game_state[move_index] = mark;
    player.pState = PlayerState::GameBlocked;

    //Check if draw occured
    if(is_game_draw(player)){
        draw_screen(*(player.game->player1));
        draw_screen(*(player.game->player2));
        return;
    }

    //Check if player won game
    if(is_game_won(player, mark)){
        if(mark == TicTacToe::X){ // X is player 2, because player 2 always starts first
            lose_screen(*(player.game->player1));
        }
        else{
            lose_screen(*(player.game->player2));
        }
        win_screen(player);
        return;
    }

    //Continue game
    update_game_state(*(player.game->player1));
    update_game_state(*(player.game->player2));
}

//Parses game related commands and runs them
void game_turn_commands(Player &player){
    if(player.game->player1 == nullptr || player.game->player2 == nullptr){
        win_screen(player);
        return;
    }

    vector<string> split_strings;
    split(player.incomingBuffer, split_strings, '|');

    map<string, int> string_index_pairs = {
        { "A1", 0 },
        { "A2", 1 },
        { "A3", 2 },
        { "B1", 3 },
        { "B2", 4 },
        { "B3", 5 },
        { "C1", 6 },
        { "C2", 7 },
        { "C3", 8 },
    };

    if(split_strings.size() != 1){
        update_game_state(player);
        return;
    }

    if(string_index_pairs.find(split_strings[0]) == string_index_pairs.end()){
        update_game_state(player);
        return;
    }

    game_make_move(player, string_index_pairs[split_strings[0]]);

    if(player.pState == PlayerState::Missing){
        return;
    }
}

int setup_socket(const int port, sockaddr_in &servaddr){
    #ifdef _WIN32
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

    if ((port < 1) || (port > 65535)){
        fprintf(stderr, "ERROR #1: invalid port specified.\n");
        return -1;
    }

    const int l_socket = socket(AF_INET, SOCK_STREAM, 0); // listening socket
    if (l_socket < 0){ //socket is a way to speak to other programs using standard Unix file descriptors. socket() returns a new socket descriptor
        fprintf(stderr, "ERROR #2: cannot create listening socket.\n");
        return -1;
    }

	servaddr = {0};
    servaddr.sin_family = AF_INET; //AF_INET (IPv4) or AF_INET6 (IPv6)
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); //INADDR_ANY binds the socket to all available interfaces, not just localhost
    servaddr.sin_port = htons(port); //htons() host to network long (network - bigendian - normaliai)

    if (bind (l_socket, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0){ //bind() IP address and port are bound to the socket. A remote machine can now connect
        fprintf(stderr,"ERROR #3: bind listening socket.\n");
        return -1;
    }

    if (listen(l_socket, MAXQUEUE) < 0){ //takes socket descriptor and tells it to listen for incoming connections
        fprintf(stderr,"ERROR #4: error in listen().\n");
        return -1;
    }

    return l_socket;
}

int main(int argc, char *argv[]){
    if (argc != 2){
        fprintf(stderr, "USAGE: %s <port>\n", argv[0]);
        return -1;
    }

    const int port = atoi(argv[1]);
    sockaddr_in servaddr;

    const int l_socket = setup_socket(port, servaddr);

	Player players[MAXCLIENTS]; //all sockets
	for(int client_id = 0; client_id < MAXCLIENTS; client_id++) {
        players[client_id].id = client_id; //assign player ids for debug
    }

    struct sockaddr_in clientaddr;
	fd_set read_set; //sockets to communicate
	int maxfd = 0;
	vector<shared_ptr<Game>> games;
    while(true){
        //Einam per visus klientus, ir jei jis ne -1, pridedam ji su FD_SET() prie stebimu klientu. Stebesime visus klientus ir listening socet'a
        FD_ZERO(&read_set);
        for (Player player : players){
            if (player.pState != PlayerState::Missing){
                FD_SET(player.c_socket, &read_set);
                maxfd = max(maxfd, player.c_socket);
            }
        }

        FD_SET(l_socket, &read_set);
        if (l_socket > maxfd){
            maxfd = l_socket;
        }

        select(maxfd + 1, &read_set, NULL , NULL, NULL); //select() allows to simultaneously check multiple sockets to see if they have data waiting to be recv()d, or if you can send() data to them without blocking

        if (FD_ISSET(l_socket, &read_set)){ //Jei listening socket'as prieme nauja connection'a
            int client_id = find_empty_user(players);
            if (client_id != -1){
                int clientaddrlen = sizeof(clientaddr);
                memset(&clientaddr, 0, clientaddrlen);

                int new_player = (int) accept(l_socket, (struct sockaddr*) &clientaddr, &clientaddrlen); // accept() sukuria naujà socketà, kuris bus dedikuotas bendrauti su ðiuo klientu
                if(new_player >= 0){
                    players[client_id].c_socket = new_player;
                    players[client_id].pState = PlayerState::Menu;

                    cout << "Player connected. Id: " << client_id << " IP: " << inet_ntoa(clientaddr.sin_addr) << endl;
                    send_menu_information(players[client_id], games);
                }
            }
        }

        //MAIN GAME LOOP
        for (Player &player : players){
            if(player.pState == PlayerState::Missing)
                continue;

            if (!FD_ISSET(player.c_socket, &read_set)){
                continue;
            }

            auto rec_status = recieve_from_socket(player);
            if(rec_status == RecieveStatus::ERR){
                continue;
            };

            //Command partially reached the server. Waiting for cycle read to fully read
            if(rec_status == RecieveStatus::PARTIAL) {
                cout << "Partial data from client " << player.id << endl;
                continue;
            }

            switch(player.pState){
                case PlayerState::Menu:
                    cout<<"Executing Menu commands"<<endl;
                    menu_commands(player, games);
                    break;
                case PlayerState::GameBlocked:
                    cout<<"Executing GameWait commands"<<endl;
                    break;
                case PlayerState::GameTurn:
                    cout<<"Executing GameTurn commands"<<endl;
                    game_turn_commands(player);
                    break;
                case PlayerState::Draw:
                case PlayerState::Win:
                case PlayerState::Loss:
                    cout<<"Executing game finished commands"<<endl;
                    player.pState = PlayerState::Menu;
                    send_menu_information(player, games);
                default:
                    break;
            }

            player.incomingBuffer = ""; //request processed
        }

        //Game cleanup loop
        for(int game_index = games.size() - 1; game_index >= 0; game_index--){
            if((games[game_index]->player1 == nullptr && games[game_index]->player2 == nullptr) ||
               (games[game_index]->finished))
            {
                cout << "Clearing game " << games[game_index]->name << "..." << endl;
                games[game_index] = nullptr;
                games.erase(games.begin() + game_index);
            }
        }
    }
    #ifdef _WIN32
    //Kad veiktu su Windows'ais
    WSACleanup();
    #endif // _WIN32

    return 0;
}
