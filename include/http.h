#ifndef __HTTP_H__
#define __HTTP_H__

#define HTTP_USERAGENT                                                                 \
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "  \
  "Chrome/86.0.4240.198 Safari/537.36"

extern TaskHandle_t http_run;

void httpTask(void *xTaskHttp);

#endif //!__HTTP_H__
