// upd 读取图片文件
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define IP "192.168.0.122"
#define PORT 5002

int main(int argc, const char* argv[])
{
    // 打开文件
    FILE* file = fopen("./new.png", "wb");
    if (file == NULL) {
        printf("open file failed\n");
        return -1;
    }

    // 设置 udp
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("create socket failed\n");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(IP);

    // 接收数据
    // 命令封装格式：/uploadImage:d,len;
    char buf[2048];
    int len = 2048; // 每次接收 1024 字节
    int sum = 0; // 接收总字节数
    // 先发送请求
    sprintf(buf, "/uploadImage:d,%d;", len);
    sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&addr, sizeof(addr));
    // 再接收数据
    while ((len = recvfrom(sockfd, buf, sizeof(buf), 0, NULL, NULL)) > 0) {
        fwrite(buf, 1, len, file);
        sum += len;
        printf("\rProgress: %d", sum);
        fflush(stdout);
    }

    fclose(file);
    close(sockfd);

    return 0;
}
