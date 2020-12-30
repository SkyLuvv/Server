#pragma once
#include <WS2tcpip.h>
#pragma comment (lib, "ws2_32.lib")
#include <string>
#include <optional>
#include <vector>
#include <sstream>
#include "Window.h"

//make sure the packet isnt padded when its x84/x64 bit
struct fileinfo
{
	char filename[1000];
    uint64_t filesize;
};


namespace Networking
{
    std::vector<char> CreateFileDescPacket(const std::string& filename, const uint64_t& filesz);
    bool StartWinSock();
    void CleanupWinSock();
}
class Server
{
public:
    Server() {};
    bool Init(int _port);
    std::optional<std::string> acceptClient();
    //assumes we are receiving a fileinfo struct if no length is specified
    bool Recv(std::vector<char>& buff, size_t length, Window& outputto);
    bool senddata(std::vector<char>& buffer, Window& outputto);
    int Poll(int timeout, bool pollout);
    bool hasConnection()const
    {
        return hasconnection;
    }
    bool SetBlocking(bool blocking);
    ~Server();
private:
    bool createListenSock();
private:
    SOCKET listensock;
    sockaddr_in serverInfo;
    int port;
    bool hasconnection = false;
private:
    std::string last_error;
    //client variables
    SOCKET connectedsock;
    sockaddr_in clientinfo;
    //send expects an int which can only hold the max value of an int. anything bigger will result
    //in unexpected behavior.
    static constexpr size_t maxbytesatatime = 2147483647; //mb
    //static constexpr size_t maxbytesatatime = 1; //mb
};
