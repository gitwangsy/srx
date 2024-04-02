// upd 发送图片文件
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define IP "192.168.0.122"
#define PORT 5002
#define THRESHOLD 2048 // 流控阈值
#define DELAY 500000 // 延时时间，单位微秒

int main(int argc, const char* argv[])
{
    // 打开文件
    FILE* file = fopen("./minimal_38.3.png", "rb");
    if (file == NULL) {
        printf("open file failed\n");
        return -1;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long total_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // 设置 udp
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("create socket failed\n");
        fclose(file);
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(IP);

    // 发送数据
    // 命令封装格式：/downloadImage:b,len,buf;
    char buf[1024];
    char sendbuf[1024 + 32];
    int len, cmdlen, sent = 0, sum = 0;
    while ((len = fread(buf, 1, sizeof(buf), file)) > 0) {
        cmdlen = sprintf(sendbuf, "/downloadImage:b,%d,", len);
        memcpy(sendbuf + cmdlen, buf, len);
        sendbuf[cmdlen + len] = ';';
        sendto(sockfd, sendbuf, cmdlen + len + 1, 0, (struct sockaddr*)&addr, sizeof(addr));

        sum += len;
        sent += len;

        // 显示进度
        printf("\rProgress: %.2f%%", (float)sum / total_size * 100);
        fflush(stdout);

        // 流控
        if (sent >= THRESHOLD) {
            usleep(DELAY);
            sent = 0;
        }

        memset(buf, 0, sizeof(buf));
        memset(sendbuf, 0, sizeof(sendbuf));
    }
    printf("\nsum: %d\n", sum);

    fclose(file);
    close(sockfd);

    return 0;
}
