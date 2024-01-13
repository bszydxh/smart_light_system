#include "main.h"
#include "utils.h"

ESPLog esp_log;

void broadcastUdpMsg(uint16_t port, String msg)
{
    sendUdpMsg("255.255.255.255", port, msg);
}
void sendUdpMsg(const char *host, uint16_t port, String msg)
{
    Udp.beginPacket(host, port); // 配置远端ip地址和端口
    Udp.print(msg);              // 把数据写入发送缓冲区
    Udp.endPacket();             // 发送数据
}