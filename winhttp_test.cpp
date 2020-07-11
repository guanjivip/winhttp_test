#include <Windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <string.h>
#include <string>
#pragma comment(lib, "winhttp")

int main()
{
    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;

    //1. 初始化一个WinHTTP-session句柄，参数1为此句柄的名称
    hSession = WinHttpOpen(L"by:guanji", NULL, NULL, NULL, NULL);
    if (hSession == NULL) {
        printf("%s", "Error:Open session failed :" + GetLastError());
        exit(-1);
    }

    //2. 通过上述句柄连接到服务器，需要指定服务器IP和端口号。若连接成功，返回的hConnect句柄不为NULL
    hConnect = WinHttpConnect(hSession, L"35.236.112.19", (INTERNET_PORT)80, 0);
    if (hConnect == NULL) {
        printf("%s", "Error:Connect failed: " + GetLastError());
        exit(-1);
    }

    
    //3. 通过hConnect句柄创建一个hRequest句柄，用于发送数据与读取从服务器返回的数据。
    hRequest = WinHttpOpenRequest(hConnect, L"GET", NULL, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    //hRequest = WinHttpOpenRequest(hConnect, L"Post", L"getServiceInfo", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    //其中参数2表示请求方式，此处为Post；参数3:给定Post的具体地址，如这里的具体地址为http://192.168.50.112/getServiceInfo
    if (hRequest == NULL) {
        printf("%s", "Error:OpenRequest failed: " + GetLastError());
        exit(-1);
    }
    
    BOOL bResults;
    bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    if (!bResults) {
        printf("%s", "Error:SendRequest failed: " + GetLastError());
    }else {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }



    ////4-1. 向服务器发送post数据
    ////(1) 指定发送的数据内容
    //data = L"This is my data to be sent";
    //const void* ss = (const char*)data.c_str();

    ////(2) 发送请求
    //BOOL bResults;
    //bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, const_cast<void*>(ss), data.length(), data.length(), 0);
    //if (!bResults) {
    //    cout << "Error:SendRequest failed: " << GetLastError() << endl;
    //    return -1;
    //}
    //else {
    //    //（3） 发送请求成功则准备接受服务器的response。注意：在使用 WinHttpQueryDataAvailable和WinHttpReadData前必须使用WinHttpReceiveResponse才能access服务器返回的数据
    //    bResults = WinHttpReceiveResponse(hRequest, NULL);
    //}

    //4-2. 获取服务器返回数据的header信息。这一步我用来获取返回数据的数据类型。
    LPVOID lpHeaderBuffer = NULL;
    DWORD dwSize = 0;
    if (bResults)
    {
        //(1) 获取header的长度
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
            WINHTTP_HEADER_NAME_BY_INDEX, NULL,
            &dwSize, WINHTTP_NO_HEADER_INDEX);

        //(2) 根据header的长度为buffer申请内存空间
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            lpHeaderBuffer = new WCHAR[dwSize / sizeof(WCHAR)];

            //(3) 使用WinHttpQueryHeaders获取header信息
            bResults = WinHttpQueryHeaders(hRequest,
                WINHTTP_QUERY_RAW_HEADERS_CRLF,
                WINHTTP_HEADER_NAME_BY_INDEX,
                lpHeaderBuffer, &dwSize,
                WINHTTP_NO_HEADER_INDEX);
        }
    }
    printf("Header contents: \n%S", lpHeaderBuffer);

    //解析上述header信息会发现服务器返回数据的charset为uft-8。这意味着后面需要对获取到的raw data进行宽字符转换。一开始由于没有意识到需要进行转换所以得到的数据都是乱码。
    //出现乱码的原因是：HTTP在传输过程中是二值的，它并没有text或者是unicode的概念。HTTP使用7bit的ASCII码作为HTTP headers，但是内容是任意的二值数据，需要根据header中指定的编码方式来描述它（通常是Content-Type header）.
    //因此当你接收到原始的HTTP数据时，先将其保存到char[] buffer中，然后利用WinHttpQueryHearders()获取HTTP头，得到内容的Content-Type,这样你就知道数据到底是啥类型的了，是ASCII还是Unicode或者其他。
    //一旦你知道了具体的编码方式，你就可以通过MultiByteToWideChar()将其转换成合适编码的字符，存入wchar_t[]中。
    //关于乱码的解决方案请看4-4

    //4-3. 获取服务器返回数据
    LPSTR pszOutBuffer = NULL;
    DWORD dwDownloaded = 0;         //实际收取的字符数
    wchar_t* pwText = NULL;
    if (bResults)
    {
        do
        {
            //(1) 获取返回数据的大小（以字节为单位）
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                printf("%s", "Error：WinHttpQueryDataAvailable failed：" + GetLastError());
                break;
            }
            if (!dwSize)    break;  //数据大小为0                

            //(2) 根据返回数据的长度为buffer申请内存空间
            pszOutBuffer = new char[dwSize + 1];
            if (!pszOutBuffer) {
                printf("%s", "Out of memory.");
                break;
            }
            ZeroMemory(pszOutBuffer, dwSize + 1);       //将buffer置0

            //(3) 通过WinHttpReadData读取服务器的返回数据
            if (!WinHttpReadData(hRequest, pszOutBuffer, dwSize, &dwDownloaded)) {
                printf("%s", "Error：WinHttpQueryDataAvailable failed：" + GetLastError());
            }
            if (!dwDownloaded)
                break;

        } while (dwSize > 0);

        //4-4. 将返回数据转换成UTF8
        DWORD dwNum = MultiByteToWideChar(CP_ACP, 0, pszOutBuffer, -1, NULL, 0);    //返回原始ASCII码的字符数目       
        pwText = new wchar_t[dwNum];                                                //根据ASCII码的字符数分配UTF8的空间
        MultiByteToWideChar(CP_UTF8, 0, pszOutBuffer, -1, pwText, dwNum);           //将ASCII码转换成UTF8
        printf("Received contents: \n%S", pwText);
    }


    //5. 依次关闭request，connect，session句柄
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
	return 0;
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

