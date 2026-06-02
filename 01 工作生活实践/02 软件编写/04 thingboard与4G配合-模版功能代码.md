> [!note] TB平台和4G配合有几种方式
> 走HTTP协议
> 	1. GET
> 	2. POST
> 走MQTT协议
# ThingBoard 与 4G HTTP 库化蓝图

这篇笔记的目标不是记录某一个具体项目怎么接入 4G，而是把 **4G HTTP 执行能力** 和 **ThingBoard HTTP 协议适配** 抽离成可复用的库化设计。后续 AI 读这篇笔记时，应优先复刻公共 HTTP 执行机，再根据项目业务提供 token、sharedKeys 和 JSON body。

## 1. 推荐拆层

### 1.1 4G HTTP Core

`4G HTTP Core` 是库的核心层，只负责“把 HTTP 请求通过 4G 模组可靠执行出去”。它不应该知道具体业务，也不应该知道 ThingBoard 的字段含义。

它负责：

- 4G 模组上电、断电、初始化。
- UART 发送、接收、分帧、清缓存。
- AT 指令状态机。
- HTTP GET / POST 的通用执行流程。
- `QHTTPCFG`、`QHTTPURL`、`QHTTPGET`、`QHTTPPOST`、`QHTTPREAD`。
- 命令响应匹配、超时、失败、忙状态。
- HTTP 状态码解析。

它不负责：

- 业务 JSON 组包。
- 传感器告警判断。
- 参数从哪里读取。
- 本地配置保存。
- 指示灯、低功耗、业务状态机。

### 1.2 ThingBoard Adapter

`ThingBoard Adapter` 是协议适配层，只负责 ThingBoard HTTP API 的路径和请求形态。

它负责：

- attributes GET URL 拼接：

```text
http://<host>:<port>/api/v1/<token>/attributes?sharedKeys=<keys>
```

- telemetry POST 请求路径拼接：

```text
POST /api/v1/<token>/telemetry HTTP/1.1
```

- POST header 中的 `Host`、`Content-Type`、`Content-Length`。
- 把 GET / POST 请求交给 `4G HTTP Core` 执行。

它不负责解析具体业务字段，也不负责生成业务 JSON。GET 取回的原始响应可以交给应用层解析，POST 的 JSON body 也由应用层传入。

### 1.3 Application Layer

`Application Layer` 是具体项目业务层。它只调用库接口，不应该反向影响 4G HTTP Core 的内部状态机。

它负责提供：

- `host`
- `port`
- `token`
- `sharedKeys`
- `json_body`
- GET 成功后的字段解析和应用
- POST 成功后的业务确认

它也负责决定什么时候发 GET、什么时候发 POST、失败后是否重试，以及成功后如何更新业务状态。

## 2. GET 与 POST 的关键差异

### 2.1 ThingBoard attributes GET

GET 适合拉取设备共享属性，例如远程配置、阈值、开关量。

推荐流程：

```text
AT+QHTTPCFG="reset"
AT+QHTTPCFG="contextid",1
AT+QHTTPCFG="requestheader",0
AT+QHTTPCFG="responseheader",0
AT+QHTTPCFG="contenttype",1
AT+QHTTPURL=<完整URL长度>,60
<写入完整 attributes URL>
AT+QHTTPGET=60
等待 +QHTTPGET: 0,200,<len>
AT+QHTTPREAD=60
读取响应 body
```

GET 的重点：

- `requestheader` 使用 `0`。
- `QHTTPURL` 后写入完整 URL。
- 成功标准优先看 `+QHTTPGET` 中的 HTTP 状态码是否为 `200`。
- 只有 GET 成功并读取到 body 后，才交给应用层解析字段。

### 2.2 ThingBoard telemetry POST

POST 适合上报遥测数据、事件、状态快照。

推荐流程：

```text
AT+QHTTPCFG="reset"
AT+QHTTPCFG="contextid",1
AT+QHTTPCFG="requestheader",1
AT+QHTTPCFG="responseheader",0
AT+QHTTPCFG="contenttype",1
AT+QHTTPURL=<根URL长度>,60
http://<host>:<port>
AT+QHTTPPOST=<HTTP请求总长度>,60,60
POST /api/v1/<token>/telemetry HTTP/1.1
Host: <host>:<port>
Content-Type: application/json
Content-Length: <body_len>

<json_body>
等待 +QHTTPPOST: 0,200,<len>
```

POST 的重点：

- `requestheader` 使用 `1`。
- `QHTTPURL` 后只写根 URL，例如 `http://host:port`。
- `QHTTPPOST=<len>` 中的 `<len>` 是“手写 HTTP 请求头 + 空行 + body”的总长度。
- `Content-Length` 只填写 body 长度。
- `Host` 必须和根 URL 的 host、port 一致。
- 只有 HTTP 状态码为 `200` 后，应用层才能确认本次上报成功。

## 3. 公共接口骨架

下面只是接口骨架，不是完整可编译源码。实现时应根据目标芯片、目标 4G 模组和现有工程风格补齐底层 HAL。

```c
typedef enum {
    G4G_RESULT_OK = 0,
    G4G_RESULT_TIMEOUT,
    G4G_RESULT_HTTP_ERROR,
    G4G_RESULT_AT_ERROR,
    G4G_RESULT_BUSY,
} g4g_result_e;

typedef void (*g4g_result_cb_t)(g4g_result_e result,
                                uint16_t http_status,
                                const char *response,
                                uint16_t response_len);

typedef struct {
    void (*power_set)(uint8_t on);
    int  (*uart_send)(const uint8_t *data, uint16_t len);
    int  (*uart_recv_frame)(uint8_t **data, uint16_t *len);
    void (*uart_clear_frame)(void);
} g4g_hal_t;

typedef struct {
    const char *host;
    uint16_t port;
    char *url_buf;
    uint16_t url_buf_size;
    char *rx_buf;
    uint16_t rx_buf_size;
} g4g_config_t;

void g4g_init(const g4g_hal_t *hal, const g4g_config_t *cfg);
void g4g_tick(uint16_t elapsed_ms);
int  g4g_task(void);
uint8_t g4g_is_busy(void);

int g4g_tb_get_attributes(const char *token,
                          const char *shared_keys,
                          g4g_result_cb_t cb);

int g4g_tb_post_telemetry(const char *token,
                          const char *json_body,
                          uint16_t body_len,
                          g4g_result_cb_t cb);
```

## 4. 状态机建议

公共库内部建议使用单一 HTTP 执行状态机，而不是 GET 和 POST 各写一套阻塞流程。

推荐状态抽象：

```text
IDLE
CHECK_REQUEST
POWER_ON
WAIT_MODULE_READY
NETWORK_ATTACH
HTTP_RESET
HTTP_CONTEXT
HTTP_REQUEST_HEADER_CFG
HTTP_RESPONSE_HEADER_CFG
HTTP_CONTENT_TYPE_CFG
HTTP_SET_URL
HTTP_SEND_URL
HTTP_SEND_GET / HTTP_SET_POST
HTTP_SEND_POST_BODY
HTTP_WAIT_RESULT
HTTP_READ_BODY
HTTP_DONE
HTTP_FAIL
POWER_OFF
PROTECT_INTERVAL
```

状态机原则：

- `g4g_tick()` 只更新时间和超时计数。
- `g4g_task()` 推进状态机，不长时间阻塞。
- UART 接收只负责入队或缓存完整帧。
- 状态机根据当前请求类型决定走 GET 分支还是 POST 分支。
- 成功或失败后通过回调通知应用层。
- 应用层不要直接改状态机内部变量。

## 5. 请求模型

库内部可以把 GET 和 POST 统一成一个请求结构。

```c
typedef enum {
    G4G_HTTP_REQ_NONE = 0,
    G4G_HTTP_REQ_TB_GET_ATTRIBUTES,
    G4G_HTTP_REQ_TB_POST_TELEMETRY,
} g4g_http_req_type_e;

typedef struct {
    g4g_http_req_type_e type;
    const char *token;
    const char *shared_keys;
    const char *json_body;
    uint16_t json_body_len;
    g4g_result_cb_t cb;
} g4g_http_request_t;
```

最小版本可以先只支持同一时间一个请求。如果 `g4g_is_busy()` 为真，新请求直接返回 `G4G_RESULT_BUSY` 或错误码。后续确实需要时，再增加请求队列。

## 6. 库边界

库内不能直接依赖：

- 具体传感器。
- 具体业务字段名。
- 全局业务变量。
- 本地参数存储。
- 指示灯。
- 低功耗策略。
- 看门狗策略。
- 生产测试逻辑。

这些内容应该由应用层处理，再通过库接口传入最小必要数据。

推荐边界是：

```text
应用层决定发什么
ThingBoard Adapter 决定请求长什么样
4G HTTP Core 决定怎么通过模组发出去
HAL 决定具体板子怎么收发 UART、怎么上下电
```

## 7. 复刻时最容易错的点

1. GET 和 POST 使用的 token 来源必须一致，不能一个走测试 token，一个走生产 token。
2. POST 的 `QHTTPPOST=<len>` 必须计算“HTTP 请求头 + 空行 + body”的总长度。
3. POST 的 `Content-Length` 只计算 body 长度。
4. POST 的 `Host` 必须与 `QHTTPURL` 写入的根 URL 一致。
5. GET 使用完整 URL，POST 使用根 URL + 手写 HTTP 请求路径，不要混用。
6. HTTP 状态码不是 AT 命令是否返回 `OK`；必须解析 `+QHTTPGET` 或 `+QHTTPPOST` 中的状态码。
7. 只有 HTTP 200 后，应用层才应该确认 GET 生效或 POST 成功。
8. JSON body 缓冲区必须预留足够长度，组包失败不能继续 POST。
9. UART 调试文本不要混入 4G 通讯串口，除非明确处于只打印、不通讯的调试模式。
10. 抽库时不要把业务 pending、告警锁存、配置落盘等策略写进公共 HTTP 状态机。

## 8. 最小复刻步骤

1. 先实现 HAL：上电、断电、UART 发送、UART 接收分帧、清缓存。
2. 再实现 `4G HTTP Core`：AT 状态机、超时、响应匹配、HTTP 状态码解析。
3. 再实现 `ThingBoard Adapter`：GET attributes URL 和 POST telemetry 请求头。
4. 再让应用层提供 token、sharedKeys、json_body。
5. 先验证 GET：能收到 `+QHTTPGET: 0,200,<len>`，并能 `QHTTPREAD` 到 body。
6. 再验证 POST：能收到 `+QHTTPPOST: 0,200,<len>`，平台能看到 telemetry。
7. 最后再把业务成功确认、失败重试、低功耗衔接放回应用层。

## 9. AI 使用提示

后续 AI 看到这篇笔记时，应遵守以下判断：

- 如果目标是复刻 ThingBoard GET/POST，优先实现公共 4G HTTP 执行机，不要复制某个项目的业务代码。
- 如果 GET 成功但配置不生效，优先查应用层解析和应用逻辑。
- 如果 POST 返回 401，优先查 token、host、路径和 ThingBoard 设备凭据。
- 如果 POST body 为空或字段不对，优先查应用层 JSON 组包。
- 如果卡在 AT 状态机，才进入 UART、供电、驻网、响应匹配和超时排查。
