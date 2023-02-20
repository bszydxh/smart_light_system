# smart_light_system
基于esp32的智能灯带系统，支持:小爱同学+流光溢彩+天气时间显示+久坐提醒+关电脑
#### ------更新于2023/2/19-------
## 更新公告
2023/2/19 支持手机配网，电脑反控灯带
## 效果演示
### 流光溢彩
https://user-images.githubusercontent.com/79249935/208168212-7c93ab88-0b14-446b-8acc-c92a9ee06c3b.mp4
### 电脑反控灯带
https://user-images.githubusercontent.com/79249935/208168375-cefdf474-2f2c-4264-8bb8-2c2823d160cd.mp4

## 项目概况
双分支开发，最新版在dev分支，不保证稳定性

master分支比较稳定

使用时注意查看esp32平台库版本是否与main.cpp的一致

手动关闭Blinker库，可实现去mqtt化运行

c#电脑端控制程序[bszydxh/udp_turn_off](https://github.com/bszydxh/udp_turn_off),作为系统的比较重要的扩展，需要自行打包

安卓控制程序[bszydxh/udp_turn_off](https://github.com/bszydxh/top.bszydxh.light),需要自行打包,并且按需要改动

感谢c#程序来源[tty228/udp_turn_off](https://github.com/tty228/udp_turn_off)

欢迎提issue

## 配置清单
    ESP32开发板 * 1

    面包板 * 1

    ws2812b 灯带 (滴胶 60灯/m) * 2m

    0.96寸 SSD1306 OLED屏 (4引脚单色) * 1

    触摸模块 * 2

    mirco转usb<u>数据线</u>

    dc母头 接线器 若干

    灯带免焊连接口(对应店家的) 若干

    快速接线端子 若干

    usb转dc线 若干

    三色导线 若干 (适配灯带)

    杜邦线 若干 (公对母,公对公,母对母)

## 从机

负责模拟电脑键盘，可以实现电脑自动解锁落锁，目前由于安全性问题暂不开源

## 支持功能
1. 小爱同学操控
2. 流光溢彩/音效律动效果(电脑端下载Prismatik软件,选择adalight,波特率256000)
3. 显示屏(一言/天气/时钟)
4. 久坐提醒(闪烁)
5. 与电脑智能联动
6. 触摸按键两个，用于开关灯/切换模式
7. 智能配网，配网所需app已开源

## 注意事项 
1. ws2812b灯带不必外接电容/电阻
2. 务必保证所有电源共地(esp32/灯带供电)
3. 禁止用esp32对灯带供电,灯带电流约5A,会烧的
4. 务必保证灯带不进水,走线不碰水
5. 保证你的esp32有多余引脚进行配置(1个或以上)
6. 触摸按键使用模块

## 常见问题
1. 引脚设置/密钥配置?
   见main.cpp注释

## 开发相关
### 技术架构

本项目基于arduino框架

在windows端 vscode platfromio平台开发

使用freertos进行任务调度

### 网络

udp进行局域网下广播收发包

1145 端口进行接收(turn_on/turn_off)

8081 向电脑端发送的端口

8082 向安卓端发送的端口
