# 脱敏版工程说明

本目录是 `Titan_npu_ai_hand_wifi` 的源码脱敏副本，原始工程未被修改。

## 已脱敏的连接配置

以下配置位于 `src/hal_entry.c`：

```c
#define WIFI_SSID         "YOUR_WIFI_SSID"
#define WIFI_PASSWORD     "YOUR_WIFI_PASSWORD"
#define VIDEO_SERVER_HOST "192.0.2.1"
#define VIDEO_SERVER_PORT 0
#define VIDEO_UPLOAD_PATH "/YOUR_WEBSOCKET_PATH"
```

`192.0.2.1` 属于文档示例地址，不会指向原项目使用的真实服务器；端口 `0` 也不会建立有效连接。构建和部署前，需要将上述值替换为自己的网络与 WebSocket Relay 配置。

本工程未发现独立的 API Key、访问令牌或网页管理密码。`VIDEO_WS_CLIENT_KEY` 仅用于 WebSocket 握手，并非身份认证密钥；脱敏副本中已替换为 RFC 示例值。

## 已排除的内容

- `Debug/`、`Release/`、`build/`、`out/` 等构建目录；
- `.o`、`.d`、`.elf`、`.hex`、`.map` 等可重新生成文件；
- Python 缓存、日志和临时文件；
- 历史源码备份；
- 工程目录内意外生成的同名完整副本；
- 与工程无关的 `%SystemDrive%`、`NVIDIA Corporation` 残留目录。
- 第三方依赖目录内的嵌套 `.git` 历史与本机 Git 身份记录；
- 自动生成的 `cmake/Generated*.cmake` 文件。

`packages`、`ra`、`rt-thread`、模型 C 源码和 RT-Thread Studio/FSP 配置均被保留，以便重新导入和构建。WHD 软件包自带的固件资源属于依赖内容，因此没有作为构建产物删除。
