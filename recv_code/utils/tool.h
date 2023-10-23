#ifndef __TOOL_H__
#define __TOOL_H__

#include <netinet/in.h> // sockaddr_in

#ifdef DEBUG // 编译时定义 -D DEBUG
#define DEBUG_PRINT(format, ...) printf(format, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(format, ...)
#endif

/**
 * @brief 设置绑定地址
 *
 * @param socketfd socket文件描述符
 * @param addr 地址
 * @param ip ip地址
 * @param port 端口
 * @return int 0:成功 -1:失败
 */
int set_bind_addr(int socketfd, struct sockaddr_in* addr, unsigned int ip, const char* port);

/**
 * @brief 获取当前时间(us)
 *
 * @return unsigned long long 当前时间(us)
 */
unsigned long long get_time_us();

/**
 * @brief 获取本地指定网卡的ip地址
 *
 * @param ifname 网卡名称
 * @param ip ip地址
 * @return int 0:成功 -1:失败
 */
int get_local_ip(const char* ifname, char* ip);

#endif // __TOOL_H__