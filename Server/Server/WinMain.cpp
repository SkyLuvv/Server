#include "Server.h"
#include <thread>
#include <chrono>
#include "FileStuff.h"





bool keeprunning = true;
void Main(Window& editwindow, Window& staticwindow);;
void Sender(Server& server, Window& staticwindow, Window& editwindow);
void Receiver(Server& server, Window& staticwindow, Window& editwindow);
bool MakeFile(std::vector<char>& filedata, const std::string& filename, Window& outputto);
std::optional<std::string> GetFile();

//callback is a calling convention
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    Window window(100, 100, 540, 250, "Annie <3 Luke", hInstance);
    Window editwindow(10, 60, 500, 140, " ", window.GetHandle(), "Edit");
    Window staticwindow(130, 5, 300, 50, " ", window.GetHandle(), "Static");

    std::thread mainthread(Main, std::ref(editwindow), std::ref(staticwindow));

    DragAcceptFiles(window.GetHandle(), TRUE);

	while (Window::ProcessMessages())
	{        
	}

    keeprunning = false;
    
}


void Main(Window & editwindow,  Window & staticwindow)
{

    using namespace std::chrono;
    using namespace std::this_thread;
    //init code
    Networking::StartWinSock();

    Server server;

    if (!server.Init(1024)) {
        staticwindow.SetText("Couldn't Init server\r\n");
        Networking::CleanupWinSock();
        return;
    }

    staticwindow.SetText("Waiting for client...\r\n");

    if (!server.acceptClient()) {
        staticwindow.SetText("Could not accept client\r\n");
        return;
    }
    

    staticwindow.SetText("Accepted Client!\r\n");

    if (!server.SetBlocking(false)) {
        staticwindow.SetText("Could not set socket to nonblocking\r\n");
        return;
    }


    std::thread sendworker(Sender, std::ref(server), std::ref(staticwindow), std::ref(editwindow));
    std::thread recvworker(Receiver, std::ref(server), std::ref(staticwindow), std::ref(editwindow));

    while (keeprunning)
    {
        sleep_for(milliseconds(1000));       
    }


    recvworker.join();
    sendworker.join();

    Networking::CleanupWinSock();
}

std::optional<std::string> GetFile()
{
    while (Window::droppedfile.empty())
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

    return { std::move(Window::droppedfile) };
}

void Sender(Server & server, Window & staticwindow, Window & editwindow)
{
    while (keeprunning)
    {

        auto filepath{ GetFile() };
        auto filename{ FileStuff::GetFilenamefromPath(filepath.value()) };

        if (!filename) {

            staticwindow.SetText("Couldnt get file name from path");
            continue;
        }

        if (MessageBox(0, filename.value().c_str(), "Do you want to send it ? <3", MB_OKCANCEL) == IDCANCEL)
            continue;

        FileStuff::File ifile(filepath.value(), filetype::read);
        if (!ifile.is_opened()) {

            staticwindow.SetText(ifile.GetLastError());
            continue;
        }

        editwindow.SetText("Gathering file data.\r\n");

        auto filedata{ ifile.GetData() };

        if (!filedata)
        {
            staticwindow.SetText(ifile.GetLastError());
            continue;
        }

        editwindow.SetText("Creating File Desc Packet.\r\n");
        auto filedesc{ Networking::CreateFileDescPacket(filename.value(), filedata.value().size()) };

        editwindow.SetText("Sending file description\r\n");
        if (!server.senddata(filedesc, editwindow)) {

            staticwindow.SetText("File description failed to send.");
            return;
        }

        editwindow.SetText("Sending file data\r\n");
        if (!server.senddata(filedata.value(), editwindow)) {
            staticwindow.SetText("File data failed to send.");
            return;
        }

    }

}
//receiving code
void Receiver(Server& server, Window& staticwindow, Window& editwindow)
{

    auto recvfile = [](Server& server, std::vector<char>& buff, fileinfo& fileinf, Window& outputto)
    {
        buff.resize(sizeof(fileinfo));
        if (!server.Recv(buff, buff.size(), outputto)) {

            outputto.SetText("Client::Recv() failed: Couldn't get file description");
            return false;
        }

        fileinfo* f = (fileinfo*)buff.data();

        //since we recevied the fileinfo struct as a vector<char>
        //copy it to the passed in fileinfo struct
        std::copy(buff.begin(), buff.end(), (char*)&fileinf);

        buff.clear();
        buff.resize(f->filesize);
        if (!server.Recv(buff, buff.size(), outputto)) {

            outputto.SetText("Server::Recv() failed: Couldn't get file data");
            return false;
        }
        return true;
    };


    while (keeprunning)
    {
        std::vector<char> buff;
        fileinfo fileinf;
        //receive both halves of the file

        if (!recvfile(server, buff, fileinf, editwindow)) continue;

        std::string filename(fileinf.filename);

        std::string str; str.reserve(100);
        if (MakeFile(buff, filename, staticwindow)) {

            str += "Successfully wrote "; str += filename; str+= " to disk";
        
            staticwindow.SetText(str);
        }

    }

}
bool MakeFile(std::vector<char>& filedata, const std::string& filename, Window& outputto)
{
    FileStuff::File ofile(filename, filetype::write);

    if (!ofile.is_opened()) {

        outputto.SetText(ofile.GetLastError());
        return false;
    }
    if (!ofile.WriteData(filedata, filedata.size())) {

        outputto.SetText(ofile.GetLastError());
        return false;
    }

    return true;
}