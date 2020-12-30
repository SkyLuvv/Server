
#include "Server.h"
#include "FileStuff.h"
#include <chrono>
#include <thread>



bool Server::Init(int _port)
{
    port = _port;
    if (createListenSock())
        return true;

    return false;

}
std::optional<std::string> Server::acceptClient()
{
    //create a new socket thats connected to the client once we found one. after this call clientinfo is filled
    // SOCKET connectedsocket = accept(listening, (sockaddr*)&clientinfo, &size);
    int sz = sizeof(clientinfo);

    if ((connectedsock = accept(listensock, (sockaddr*)&clientinfo, &sz)) == INVALID_SOCKET)
        return {};
    else
    {
        char clientipaddr[100];

        //The InetNtop function converts an IPv4 or IPv6 Internet network address into a string in Internet standard format
        //clientinfo is a structure that contains multiple structs with info of the client
        //if Family parameter is AF_INET, then the pAddr parameter must point to an IN_ADDR structure with the IPv4 address to convert.
        inet_ntop(AF_INET, &clientinfo.sin_addr, clientipaddr, sizeof(clientipaddr));

        hasconnection = true;
        return std::string(clientipaddr);
    }

}
bool Server::Recv(std::vector<char>& buff, size_t length, Window& outputto)
{
    size_t totalbytesrecv = 0;
    size_t bytesrecv = 0;
    std::string str; str.reserve(200);


    while (totalbytesrecv != length)
    {
        auto result{ Poll(10000,false) };

        if (result == SOCKET_ERROR) {
            outputto.SetText("Client::senddata failed: Poll return SOCKET_ERROR\r\n");
            return false;
        }
        else if (result == 0)           //recv buffer has no data to receive
            continue;
        else if (result == 1) {         //theres data available to be received

            bytesrecv = recv(connectedsock, buff.data() + totalbytesrecv, buff.size() - totalbytesrecv, 0);
            if (bytesrecv == SOCKET_ERROR || bytesrecv == 0)  //0 means connection closed
                return false;
            totalbytesrecv += bytesrecv;


            str += "Received "; str += std::to_string(((float)totalbytesrecv / (float)length) * 100.0f); str += "%";
            outputto.SetText(str);
            str.clear();
        }

    }

    return true;
}
bool Server::SetBlocking(bool blocking)
{
    unsigned long nonBlocking = 1;
    unsigned long Blocking = 0;
    //if passed in true, set to blocking. otherwise set to nonblocking
    auto result{ ioctlsocket(connectedsock, FIONBIO, (blocking) ? &Blocking : &nonBlocking) };

    return (result == SOCKET_ERROR) ? false : true;
}
Server::~Server()
{
    closesocket(listensock);
    closesocket(connectedsock);
}
bool Server::createListenSock()
{
    //AF_INET The Internet Protocol version 4 (IPv4)address family.
    //SOCK_STREAM A socket type that provides sequenced, reliable, two-way,
    //connection-based byte streams with an OOB data transmission mechanism.
    if ((listensock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
        return false;

    //make it use ipv4 which i guess is basically what most connections use
    serverInfo.sin_family = AF_INET;
    //htons converts the port from host to network byte order. because they use different formats or endianness
    serverInfo.sin_port = htons(port);
    //specify that we can accept any ip address
    serverInfo.sin_addr.S_un.S_addr = INADDR_ANY;

    //bind the listen socket to the connection info so it knows what to do and what port to listen on etc.
    if (bind(listensock, (sockaddr*)&serverInfo, sizeof(serverInfo)) == SOCKET_ERROR)
        return false;

    //The listen function places a socket in a state in which it is listening for an incoming connection.
    //SOMAXCONN is the maximum length of the queue of pending connections we can have
    if (listen(listensock, SOMAXCONN) == SOCKET_ERROR)
        return false;

    //if it returns an empty container, that means it was successful
    return true;

}
bool Server::senddata(std::vector<char>& buffer, Window& outputto)
{
    size_t totalbytessent = 0;
    size_t length = buffer.size();
    size_t bytessent = 0;
    std::string str; str.reserve(200);


    while (true)
    {

        auto result{ Poll(10000,true) };

        if (result == SOCKET_ERROR) {
            outputto.SetText("Server::senddata failed: Poll returned SOCKET_ERROR\r\n");
            return false;
        }
        else if (result == 0)          //means the send buffer has not sent all the data we passed yet and doesnt have space
            continue;
        else if (result == 1)          //means the send buffer has space
        {

            if (totalbytessent != length)
            {
                if ((length - totalbytessent) > maxbytesatatime)
                    bytessent = send(connectedsock, buffer.data() + totalbytessent, maxbytesatatime, 0);
                else
                    bytessent = send(connectedsock, buffer.data() + totalbytessent, length - totalbytessent, 0);

                if (bytessent == SOCKET_ERROR)
                    return false;
                totalbytessent += bytessent;

                
                str += "Sent "; str += std::to_string(((float)totalbytessent / (float)length) * 100.0f); str += "%";
                str += " to transport layer. The data is being received...";
                outputto.SetText(str);
                str.clear();

            }
            else
                break;
        }

    }
    
        return true;
}

int Server::Poll(int timeout, bool pollout)
{
    static WSAPOLLFD pollfd{};
    pollfd.fd = connectedsock;
    pollfd.events = (pollout) ? POLLOUT : POLLIN;

    auto num_events{ WSAPoll(&pollfd, 1, timeout) };

    if (num_events == 0)
        return 0;
    if (num_events == SOCKET_ERROR)
        return SOCKET_ERROR;
    if (pollfd.revents & ((pollout) ? POLLOUT : POLLIN))
        return 1;
}

std::vector<char> Networking::CreateFileDescPacket(const std::string& filename, const uint64_t& filesz)
{
    //create the packet structure we want to send which contains the file
    //information
    fileinfo fileinf{};
    //copy the filename into the struct
    filename.copy((char*)&fileinf, filename.length());
    fileinf.filesize = filesz;
    //now copy the fileinf structure into a vector<char> we can send passing beg and end of the fileinfo struct
    return std::vector<char>((char*)&fileinf, (char*)&fileinf + sizeof(fileinf));
}

bool Networking::StartWinSock()
{
    WSADATA wsData;
    int version{ MAKEWORD(2, 2) };

    if (WSAStartup(version, &wsData) == 0)
        return true;

    return false;
}

void Networking::CleanupWinSock()
{
    WSACleanup();
}
