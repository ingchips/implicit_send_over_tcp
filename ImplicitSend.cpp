#define WINVER 0x0501 // Windows XP
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <Ws2tcpip.h>

typedef unsigned char uint8_t;

#include "implicitsendapi.h"

namespace base64
{
    std::string encode(const void *raw_data, int data_len, bool for_url = false)
    {
        const uint8_t *data = (const uint8_t *)raw_data;
        int len = (data_len + 2) / 3 * 4;
        int i, j;
        const char padding = for_url ? '.' : '=';
        std::string r;
        r.resize(len);
        char *res = (char *)r.data();

        const char *base64_table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        for(i = 0, j = 0; i < len - 2; j += 3, i += 4)
        {
            res[i + 0] = base64_table[data[j] >> 2];
            res[i + 1] = base64_table[(data[j] & 0x3) << 4 | (data[j + 1] >> 4)];
            res[i + 2] = base64_table[(data[j + 1] & 0xf) << 2 | (data[j + 2] >> 6)];
            res[i + 3] = base64_table[data[j + 2] & 0x3f];
        }

        switch (data_len % 3)
        {
            case 1:
                res[i - 2] = padding;
                res[i - 1] = padding;
                break;
            case 2:
                res[i - 1] = padding;
                break;
        }

        return r;
    }
}

class Connection
{
public:
    Connection() : line_end('\n'), client_sockfd(INVALID_SOCKET), connected(false)
    {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2,2), &wsaData);
    }

    int reestablish(void)
    {
        if (connected) return 0;

        if ((client_sockfd = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
            return 2;

        if (connect(client_sockfd, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) < 0)
            return 2;
        connected = true;
        return 0;
    }

    int establish(u_short port = 9168, const char *addr = "127.0.0.1")
    {
        memset(&remote_addr, 0, sizeof(remote_addr));
        remote_addr.sin_family = AF_INET;
        remote_addr.sin_addr.s_addr = inet_addr(addr);
        remote_addr.sin_port = htons(port);

        return reestablish();
    }

    int send_and_receive(const char *request, std::string &response)
    {
        int r;
        response.clear();

        r = reestablish();
        if (r != 0) return r;

        std::string encoded = base64::encode(request, strlen(request));

        r = send_data(encoded.c_str(), (int)encoded.size());
        if (r != 0) return r;
        r = send_data(&line_end, sizeof(line_end));
        if (r != 0) return r;

        return recv_data(response);
    }

protected:

    void cleanup(void)
    {
        if (client_sockfd != INVALID_SOCKET)
            closesocket(client_sockfd);
        client_sockfd = INVALID_SOCKET;
        connected = false;
    }

    int send_data(const char *buf, int size)
    {
        const char *p = buf;
        int len = size;
        while (len > 0)
        {
            int c = send(client_sockfd, p, len, 0);
            if (c < 0)
            {
                cleanup();
                return -1;
            }
            if (c == 0)
                continue;
            len -= c;
            p += c;
        }
        return 0;
    }

    int recv_data(std::string &response)
    {
        char ch;

        while (true)
        {
            int len = recv(client_sockfd, &ch, 1, 0);
            if (len == -1)
            {
                switch (WSAGetLastError())
                {
                case WSAENETDOWN:
                case WSAENOTCONN:
                case WSAENETRESET:
                case WSAESHUTDOWN:
                case WSAECONNABORTED:
                case WSAETIMEDOUT:
                case WSAECONNRESET:
                    cleanup();
                    return -1;
                default:
                    break;
                }
            }

            if (len == 1)
            {
                if (line_end == ch)
                {
                    printf("res: %s\n", response.c_str());
                    return 0;
                }
                response.push_back(ch);
            }
        }
    }

protected:    
    const char line_end;
    SOCKET client_sockfd;
    bool connected;
	struct sockaddr_in remote_addr;
};

static std::string persistent_result;
static Connection connection;

bool WINAPI InitImplicitSend()
{
    if (connection.establish() != 0)
        return false;

    std::string response;
    if (connection.send_and_receive("InitImplicitSend", response) != 0)
        return false;

    return response == "True";
}

void WINAPI ImplicitStartTestCase(
    std::string &strTestCaseName)
{
    std::string response;
    char command[1024];
    sprintf(command, "ImplicitStartTestCase:%s", strTestCaseName.c_str());

    connection.send_and_receive(command, response);
}

char *WINAPI ImplicitSendStyle(
    std::string &strMmiText,
    UINT mmiStyle)
{
    char command[10240];
    sprintf(command, "ImplicitSendStyle:%u,%s", mmiStyle, strMmiText.c_str());

    connection.send_and_receive(command, persistent_result);
    return (char *)persistent_result.data();
}

char *WINAPI ImplicitSendPinCode(void)
{
    connection.send_and_receive("ImplicitSendPinCode", persistent_result);
    return (char *)persistent_result.data();
}

void WINAPI ImplicitTestCaseFinished()
{
    std::string response;
    connection.send_and_receive("ImplicitTestCaseFinished", response);
}

char *WINAPI ImplicitSendStyleEx(std::string &strMmiText, UINT mmiStyle, std::string &strBdAddr)
{
    char command[10240];
    sprintf(command, "ImplicitSendStyleEx:%u,%s,%s", mmiStyle, strBdAddr.c_str(), strMmiText.c_str());

    connection.send_and_receive(command, persistent_result);
    return (char *)persistent_result.data();
}

char *WINAPI ImplicitSendPinCodeEx(std::string &strBdAddr)
{
    char command[10240];
    sprintf(command, "ImplicitSendPinCodeEx:%s", strBdAddr.c_str());

    connection.send_and_receive(command, persistent_result);
    return (char *)persistent_result.data();
}
