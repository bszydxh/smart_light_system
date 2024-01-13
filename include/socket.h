#ifndef __SOCKET_H__
#define __SOCKET_H__

extern TaskHandle_t udp_run;
extern TaskHandle_t tcp_run;
extern TaskHandle_t udp_config_run;

extern WiFiUDP Udp;

void udpTask(void *xTaskUdp);
void tcpTask(void *xTaskTcp);
void udpConfigTask(void *xTaskConfigUdp);

#endif //!__SOCKET_H__
