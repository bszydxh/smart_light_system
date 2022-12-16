# smart_light_system
#### ------更新于2022/12/17-------
## 效果演示

https://user-images.githubusercontent.com/79249935/208167465-ba8f5479-bb8d-4373-8cfc-ede13e6bcc22.mp4

## 写在前面
整个项目立于2021-11-10


起因是b站上突然发现的视频

视频就不放了

    关键词:esp8266 舵机 开灯

然后在cubegarden群(某个mc屑服务器)友好的交流之后

就忍痛不氪了原神小月卡(

买了个实体月卡(不是)

心想着欸↗,美滋滋

得...坐牢的开始 (www

## 项目概况
双分支开发，最新版在dev分支，不保证稳定性

master分支比较稳定

本项目基于arduino框架

vscode platfromio平台开发

使用freertos进行任务调度

udp进行局域网下广播收发包

1145端口进行接收(turn_on/turn_off)

8080端口进行发送(用于反向与电脑联调）

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

另一块esp32将于近期开源，负责模拟电脑键盘
可以实现电脑自动解锁落锁

## 支持功能
1. 小爱同学操控
2. 流光溢彩/音效律动效果(电脑端下载Prismatik软件,选择adalight,波特率460800)
3. 显示屏(一言/天气/时钟)
4. 久坐提醒(闪烁)
5. 与电脑智能联动
6. 触摸按键两个，用于开关灯/切换模式
7. 智能配网，配网所需app已开源（稳定版不支持，支持配网特性的在spark分支）

## 注意事项 
1. ws2812b灯带不必外接电容/电阻
2. 务必保证所有电源共地(esp32/灯带供电)
3. 禁止用esp32对灯带供电,灯带电流约5A,会烧的
4. 务必保证灯带不进水,走线不碰水
5. 保证你的esp32有多余引脚进行配置(1个或以上)
6. 触摸按键使用模块

## TODO LIST

1. 优化响应机制

## 已知bug
- 稳定复现
  1. usb模式/电视(关电脑)模式下关灯会先亮灯之后关闭(逻辑错误)
## Q&A(自问自答)
1. 至于第一块? 

    别问,问就是5v的电压接在了3.3v的引脚上了把这东西击穿了(手贱),显示屏也烧了
    
        这esp32十分的珍贵
    
    一片30CNY , 嘤嘤嘤
2. 引脚设置/密钥配置?

   见main.cpp注释

