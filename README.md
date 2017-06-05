# cqpmq [![Build status](https://ci.appveyor.com/api/projects/status/iphnopjiy4cnlu8i?svg=true)](https://ci.appveyor.com/project/everpcpc/cqpmq)

将 CoolQ 收到的消息全部转发到 Beanstalkd，同时从 Beanstalkd 读取消息并处理。

## 配置
默认配置文件为 `app/com.everpcpc.cqpmq/config.ini`
```ini
[btd]
addr=127.0.0.1
port=11300
```

官方网站
--------
主站：https://cqp.cc

文库：https://d.cqp.me
