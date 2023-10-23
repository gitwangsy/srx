#include "cmd.h"
#include "tool.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h> // malloc
#include <string.h>
#include <sys/time.h>
#include <unistd.h> // close

int send_cmd_broadcast(const char* cmd)
{
    int cmd_socketfd; // 指令套接字
    struct sockaddr_in cmd_addr; // 指令地址
    int broadcast = 1; // 广播标志
    int ret;

    // 创建套接字
    cmd_socketfd = socket(AF_INET, SOCK_DGRAM, 0); // UDP
    if (cmd_socketfd < 0) {
        PTR_PERROR("socket cmd error");
        return -1;
    }
    // 设置套接字选项
    ret = setsockopt(cmd_socketfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)); // 广播
    if (ret < 0) {
        PTR_PERROR("setsockopt cmd error");
        return -1;
    }

    // 设置地址
    memset(&cmd_addr, 0, sizeof(cmd_addr));
    cmd_addr.sin_family = AF_INET;
    cmd_addr.sin_port = htons(atoi(CMD_PORT));
    cmd_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    // 发送指令
    ret = sendto(cmd_socketfd, cmd, strlen(cmd), 0, (struct sockaddr*)&cmd_addr, sizeof(cmd_addr)); // 广播
    if (ret < 0) {
        PTR_PERROR("sendto cmd error");
        return -1;
    }

    PTR_DEBUG("send_cmd_broadcast: %s\n", cmd); // 打印发送的数据包

    // 关闭套接字
    close(cmd_socketfd);

    return 0;
}

int send_cmd_unicast(struct sockaddr_in* addr, const char* cmd)
{
    int cmd_socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (cmd_socketfd < 0) {
        PTR_PERROR("socket cmd error");
        return -1;
    }

    // 发送指令
    if (sendto(cmd_socketfd, cmd, strlen(cmd), 0, (struct sockaddr*)addr, sizeof(*addr)) < 0) {
        PTR_PERROR("sendto cmd error");
        return -1;
    }

    PTR_DEBUG("send_cmd_unicast: %s\n", cmd); // 打印发送的数据包

    // 关闭套接字
    close(cmd_socketfd);

    return 0;
}

int recv_cmd(struct sockaddr_in* addr, char* cmd)
{
    int cmd_socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (cmd_socketfd < 0) {
        PTR_PERROR("socket cmd error");
        return -1;
    }

    struct sockaddr_in cmd_addr; // 指令地址

    // 设置端口复用
    int opt = 1;
    if (setsockopt(cmd_socketfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        PTR_PERROR("setsockopt cmd error");
        return -1;
    }

    if (set_bind_addr(cmd_socketfd, &cmd_addr, INADDR_ANY, CMD_PORT) < 0) {
        PTRERR("set_bind_addr cmd error");
        return -1;
    }

    // 接收指令
    if (recvfrom(cmd_socketfd, cmd, CMD_BUF_SIZE, 0, (struct sockaddr*)addr, (socklen_t*)sizeof(*addr)) < 0) {
        PTR_PERROR("recvfrom cmd error");
        return -1;
    }

    PTR_DEBUG("recv_cmd: %s\n", cmd);

    // 关闭套接字
    close(cmd_socketfd);

    return 0;
}

char ip[128][16] = { 0 }; // 组网中所有节点 ip 数组
int net_id = 0; // 组网 ID
char local_ip[16] = { 0 }; // 本机 IP 地址
int node_num = 0; // 组网中节点数量
unsigned long long* detect_time = NULL; // 探测时间

int cmd_handler(struct sockaddr_in* addr, const char* cmd)
{
    if (!strcmp(cmd, CMD_GET_IP)) { // 获取节点 IP
        char ip_buf[16] = { 0 };
        get_local_ip("eth0", ip_buf); // 获取本机IP地址
        send_cmd_broadcast(ip_buf);
    } else if (!strncmp(cmd, CMD_SET_NETWORK_ID(0, 0), 16)) { // 选择设备进行组网，并分配组网ID
        // 字符串解析
        char ip_buf[16] = { 0 }; // IP 地址
        int net_id_buf = 0; // 组网 ID
        sscanf(cmd, CMD_SET_NETWORK_ID(% s, % i), ip_buf, &net_id);

        if (is_ip(ip_buf) && net_id_buf > 0) { // 判断是否为 IP 地址和组网 ID
            // 判断是否为本机 IP 地址
            if (!strcmp(ip_buf, local_ip)) { // 如果是本机 IP 地址
                if (net_id_buf != net_id) { // 如果组网 ID 不同
                    // 清空 IP 地址
                    memset(ip, 0, sizeof(ip));
                    node_num = 0;
                    // 设置组网 ID
                    net_id = net_id_buf;
                    PTR_DEBUG("set network id: %d\n", net_id);
                }
            }
            // 组网内所有节点同步组网中的所有节点 IP 地址
            if (net_id_buf == net_id) {
                // 组网 ID 相同，保存 IP 地址
                for (int i = 0; i < sizeof(ip) / sizeof(ip[0]); i++) {
                    if (!strcmp(ip[i], ip_buf)) {
                        // 已存在，跳过
                        break;
                    }
                    if (!strcmp(ip[i], "")) {
                        // 不存在，保存
                        strcpy(ip[i], ip_buf);
                        node_num++;
                        break;
                    }
                }
                // 再发送自己的 IP 地址和组网 ID，与其他节点同步
                char cmd_buf[CMD_BUF_SIZE] = { 0 };
                sprintf(cmd_buf, CMD_SET_NETWORK_ID(% s, % d), local_ip, net_id);
                send_cmd_broadcast(cmd_buf);
            }
        }
    } else if (!strcmp(cmd, CMD_DETECT_TIME_GAP)) { // 检测时间差
        if (send_cmd_unicast(addr, CMD_ACK_TIME_GAP) < 0) {
            PTRERR("send_cmd_unicast error");
            return -1;
        }
    } else if (!strcmp(cmd, CMD_ACK_TIME_GAP)) { // 应答时间差
        struct timeval tv;
        unsigned long long ack_time; // 应答时间
        gettimeofday(&tv, NULL);
        ack_time = tv.tv_sec * 1000000 + tv.tv_usec; // us
        PTR_DEBUG("recv_ack: %s, time: %llu\n", cmd, ack_time); // 打印接收的数据包
        // 查找 IP 地址对应的数组下标
        int index = -1;
        for (int i = 0; i < node_num; i++) {
            if (!strcmp(ip[i], inet_ntoa(addr->sin_addr))) {
                index = i;
                break;
            }
        }
        if (index < 0) {
            PTRERR("ip not found");
            return -1;
        }
        // 计算时间差
        unsigned long long gap = (ack_time - detect_time[index]) / 2;
        PTR_DEBUG("gap: %llu\n", gap);
        // 设置时间差
        char cmd_buf[CMD_BUF_SIZE] = { 0 };
        sprintf(cmd_buf, CMD_SET_TIME_GAP(% llu), gap);
        if (send_cmd_unicast(addr, cmd_buf) < 0) {
            PTRERR("send_cmd_unicast error");
            return -1;
        }
    } else if (!strncmp(cmd, CMD_SET_TIME_GAP(0), 13)) { // 设置时间差
        // 字符串解析
        unsigned long long gap = 0; // 时间差
        sscanf(cmd, CMD_SET_TIME_GAP(% llu), &gap);
        PTR_DEBUG("gap: %llu\n", gap);
        // TODO
    } else if (!strcmp(cmd, CMD_START)) { // 开始指令
        // TODO
    } else {
        PTR_DEBUG("unknown cmd: %s\n", cmd);
    }

    return 0;
}

int detect_time_gap()
{
    if (node_num < 2) {
        return 1; // 组网中节点数量小于 2，无法进行时间校正，等待其他节点加入
    }

    // 向组网中的除自己以外的所有节点发送探测时间差指令
    int cmd_socketfd; // 指令套接字
    struct sockaddr_in cmd_addr[node_num]; // 指令地址

    // 创建套接字
    cmd_socketfd = socket(AF_INET, SOCK_DGRAM, 0); // UDP
    if (cmd_socketfd < 0) {
        PTR_PERROR("socket cmd error");
        return -1;
    }

    // 设置地址
    for (int i = 0; i < node_num; i++) {
        memset(&cmd_addr[i], 0, sizeof(cmd_addr[i]));
        /* if (!strcmp(ip[i], local_ip)) {
            continue; // 跳过自己
        } */
        cmd_addr[i].sin_family = AF_INET;
        cmd_addr[i].sin_port = htons(atoi(CMD_PORT));
        cmd_addr[i].sin_addr.s_addr = inet_addr(ip[i]);
    }

    // 发送指令
    struct timeval tv;
    detect_time = (unsigned long long*)malloc(sizeof(unsigned long long) * node_num); // 探测时间
    if (detect_time == NULL) {
        PTR_PERROR("malloc error");
        return -1;
    }

    for (int i = 0; i < node_num; i++) {
        gettimeofday(&tv, NULL);
        detect_time[i] = tv.tv_sec * 1000000 + tv.tv_usec; // us
        if (sendto(cmd_socketfd, CMD_DETECT_TIME_GAP, strlen(CMD_DETECT_TIME_GAP), 0, (struct sockaddr*)&cmd_addr[i], sizeof(cmd_addr[i])) < 0) {
            PTR_PERROR("sendto cmd error");
            return -1;
        }
        PTR_DEBUG("send_detect: %s, time: %llu\n", CMD_DETECT_TIME_GAP, detect_time[i]); // 打印发送的数据包
    }

    sleep(1); // 等待 1s

    free(detect_time);
    detect_time = NULL;
    close(cmd_socketfd);

    return 0;
}