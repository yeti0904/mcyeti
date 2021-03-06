#include <thread>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dotproperties.hh>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include "fs.hh"
#include "util.hh"
#include "worker.hh"
#include "heartbeat.hh"
#include "player.hh"
#include "worker_utils.hh"
#include "level.hh"
#include "console.hh"
#define timen currentTime().c_str()
using std::thread;
using std::vector;
using std::ref;

int main(void) {
	// Properties
	Properties props;
	if (fexists("server.properties")) {
		props.readFromFile("server.properties");
	}
	else {
		props["name"]         = "[MCYeti] Default";
		props["port"]         = "25565";
		props["motd"]         = "Hello!";
		props["public"]       = "true";
		props["heartbeatURL"] = "https://www.classicube.net/heartbeat.jsp";
		props["maxPlayers"]   = "12";
		fcreate("server.properties");
		f_write("server.properties", props.stringify());
		printf("[%s] Created server.properties\n", timen);
	}
	if (!pexists("players")) {
		pcreate("players");
	}

	// Startup
	int sock;
	printf("[%s] Starting server\n", timen);
	
	struct addrinfo* addrInfoResult = nullptr;
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, props["port"].c_str(), &hints, &addrInfoResult) != 0) {
		std::cerr << "Failed to initialize TCPSocket: getaddr info failed" << std::endl;
		std::exit(1);
	}
	
	sock = socket(addrInfoResult->ai_family, addrInfoResult->ai_socktype, addrInfoResult->ai_protocol);
	if (sock == -1) {
		printf("[%s] ERROR! Failed to create socket", timen);
		return 1;
	}
	printf("[%s] Successfully created socket\n", timen);

	const int32_t reuse = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
		printf("[%s] Failed to set socket option to reuse address.", timen);
		return 1;
	}

	if (bind(sock, addrInfoResult->ai_addr, static_cast<int>(addrInfoResult->ai_addrlen)) == -1) {
		printf("[%s] Failed to bind to address\n", timen);
		return 1;
	}
	printf("[%s] Binded to address\n", timen);

	if (listen(sock, 10) == -1) {
		printf("[%s] Failed to listen on address\n", timen);
	}
	sockaddr_in server_info = { 0 };
	socklen_t saddrsize = sizeof(server_info);
	getpeername(sock, reinterpret_cast<sockaddr*>(&server_info), &saddrsize);
	printf("[%s] Startup complete\n", timen);
	printf("[%s] Listening on %s:%s\n", timen, inet_ntoa(server_info.sin_addr), props["port"].c_str());

	// attempt to port forward with upnp
	printf("[%s] Attempting upnp port forwarding\n", timen);
	openupnpport(props["port"]);

	// variables
	string   serverSalt    = generateSalt();

	// Listen loop
	bool            run = true;
	int             isock;
	player          iclient;
	vector <thread> client_threads;
	vector <player>    clients;
	sockaddr_in     client_info = { 0 };
	socklen_t       addrsize = sizeof(client_info);
	string          client_ip;

	printf("Owner mppass: %s\n", md5(serverSalt + "MESYETI").c_str());
	printf("0904 mppass: %s\n", md5(serverSalt + "yeti0904").c_str());
	printf("[%s] Generated server salt: %s\n", timen, serverSalt.c_str());

	// set up console
	thread consoleThread(console::worker, ref(clients), ref(run));
	printf("[%s] Started console worker\n", timen);
	
	// Set up heartbeat
	thread heartbeatThread;
	if (props["public"] == "true") {
		heartbeatThread = thread(heartbeat, props, serverSalt, ref(clients), ref(run));
	}
	printf("[%s] Started heartbeat thread\n", timen);

	unordered_map <string, level> maps;
	maps["main"].generate(0, 0, 0);
	printf("[%s] Generated level\n", timen);

	while (run) {
		isock = accept(sock, NULL, NULL);
		iclient.sock = isock;
		getpeername(isock, reinterpret_cast<sockaddr*>(&client_info), &addrsize);
		client_ip = inet_ntoa(client_info.sin_addr);
		printf("[%s] %s connected to the server.\n", timen, client_ip.c_str());
		if (clients.size() >= atoi(props["maxPlayers"].c_str())) {
			worker::disconnectClient(iclient, clients, "Too many players", "");
		}
		else 
		client_threads.push_back(thread(client::worker, iclient, maps, props, serverSalt, ref(clients)));
	}

	if (props["public"] == "true") heartbeatThread.join();
	for (size_t i = 0; i<client_threads.size(); ++i) {
		client_threads[i].detach();
	}

	// end of program
	return 0;
}
