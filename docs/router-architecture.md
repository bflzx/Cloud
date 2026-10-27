# 路由分层设计(学习用)

> 本文是 [整体实施方案](overall-plan.md) 与 [地基修复计划](foundation-plan.md) 的补充折出页:
> 把 `http_server.hpp` 里那个"什么都干"的 `router_handle` 拆成清晰的分层。
> **只讲结构与思路,不提供可直接粘贴的实现代码。**
> 主要服务于 M0 步骤 5/6,但其中的分层骨架会一直用到 M7。

---

## 0. 一句话目标

> 把"**会变的业务**"从"**不变的管道**"里赶出去。
> 验收标准:**加一个新接口 = 只新增代码,不改已有代码。**

---

## 1. 病根:一个函数干了五件事

现状 `server/http_server.hpp` 的 `router_handle` 同时承担了:

1. **路由匹配** —— 一条长长的 `if / else if` 链
2. **鉴权判定** —— 白名单 + 读 `Authorization` 头
3. **业务逻辑** —— 登录 `SELECT`、PBKDF2 校验、注册 `INSERT`
4. **响应组装** —— 手写 `resp["code"] = ...`(同一模式重复约 8 次)
5. **HTTP 细节** —— 设 `Content-Type`、`prepare_payload`、读 `req.body()`

**后果:改业务必须动框架代码。** 每加一个接口,就得在这五件事的"缝合处"再插一刀 ——
这就是"想加逻辑只能不停加 `else if`"的根因。**不是逻辑无处安放,而是"放逻辑的地方"和"框架管道"是同一段代码。**

---

## 2. 心智转变:从「写分支」到「填表格」

| | 现在的思维 | 目标思维 |
|---|---|---|
| 形式 | "如果是这个路径,就做这件事" | "把接口登记进一张表,请求来了查表" |
| 代码形态 | `if / else if` 分支 | 数据(路由表)驱动 |
| 加接口 | 改**框架**代码 | 加一行注册 + 写一个 handler |
| 本质 | 接口清单写在**代码**里 | 接口清单变成**数据** |

---

## 3. 四层职责

自上而下四层,每层的"会变程度"不同:

| 层 | 代表 | 职责 | 会变吗 |
|---|---|---|---|
| **传输层** | `client_handle` | 收字节 / 发字节 / keep-alive | 基本不变 |
| **路由层** | `Router` | 匹配 → 鉴权 → 造上下文 → 调 handler → 发响应 | 很少变 |
| **业务层** | `handler` | 只回答"这个请求的业务结果是什么" | 经常变 |
| **数据层** | `repo / service` | 数据存取与业务规则 | 经常变 |

**铁律:上层知道下层,下层不认识上层。**

- 传输层不知道任何业务;
- handler **不知道 HTTP 细节,也不碰 socket**;
- 路由层不认识任何具体 handler(见 7.4)。

---

## 4. 分层总览图

```text
                  ┌───────────────────────────────────────────────┐
   TCP 连接 ─────▶│  client_handle                    (传输层·稳定) │
                  │  只做: 读请求 / 写响应 / keep-alive           │
                  └───────────────────────┬───────────────────────┘
                                          │ 原始 req / res
                                          ▼
                  ┌───────────────────────────────────────────────┐
                  │  Router::dispatch                 (路由层·稳定) │
                  │  1) 用 (method, path) 查表                    │
                  │  2) 命中?  否 ─▶ 404                          │
                  │  3) 路由项 requires_auth?                     │
                  │       是 ─▶ 验 token,失败立即 401(不进 handler)│
                  │       否 ─▶ 跳过                              │
                  │  4) 把 uid 等塞进 Context                     │
                  │  5) 调用 handler(ctx) ─▶ 拿到 Response        │
                  │  6) 统一序列化并写回                          │
                  └───────────────────────┬───────────────────────┘
                                          │ Context (入) / Response (出)
                                          ▼
                  ┌───────────────────────────────────────────────┐
                  │  handler(ctx)                     (业务层·常变) │
                  │  handle_login / handle_register / handle_...  │
                  │  不碰 HTTP、不碰 socket、不拼报文              │
                  └───────────────────────┬───────────────────────┘
                                          │
                                          ▼
                  ┌───────────────────────────────────────────────┐
                  │  repo / service                   (数据层·常变) │
                  └───────────────────────────────────────────────┘
```

---

## 5. 单次请求流转图(以登录为例)

```text
 POST /api/login
      │
      ▼
 [查表] 命中 (POST, /api/login)
      │
      ▼
 requires_auth = false  ──▶ 跳过鉴权
      │
      ▼
 handle_login(ctx)
      │
      ├─▶ repo::find_user(username)      (数据层)
      │        │
      │        └─ 不存在 → 返回 业务失败(用户不存在)
      │
      ├─▶ 校验 PBKDF2 哈希
      │        │
      │        └─ 不匹配 → 返回 业务失败(密码错误)
      │
      └─▶ 签发 token → 返回 成功( data.token )
      │
      ▼
 Router 统一:设状态码 + Content-Type + 序列化
      │
      ▼
 写回 socket
```

关键:**handler 只产出"结果",不产出"字节"。** 把结果变成字节是路由层的事。

---

## 6. 核心数据结构图

```text
Router
 └── map< Key , Route >
        │
        ├── Key   = ( http::verb method , std::string path )   ← 必须带 method!
        │
        └── Route = {
                      bool    requires_auth ,   ← 鉴权变成"路由的属性"
                      Handler handler           ← 统一的函数签名
                    }

 Handler  = std::function< Response(const Context&) >        ← 同步版(推荐先做)
         或 std::function< awaitable<Response>(Context) >     ← 异步版(以后再说,见 7.1)

 Context  = { method, path, query, headers, body, uid }       ← handler 的唯一入口
 Response = { status, code, message, data, headers }          ← handler 的唯一出口
```

> 注意:这里的 `Context` / `Response` 是**你自己定义的类型**,不是 `beast::http::request/response`。
> 这个区别就是"业务层不认识 HTTP"的具体体现。

---

## 7. 关键决策点(四个)

### 7.1 最重要:handler 是同步还是异步?

**这是整个方案能不能落地的关键。** 你的是协程项目,handler 将来要 `co_await`(异步查 DB、流式读 body),
而**普通的同步 `std::function` 装不下"会挂起"的 handler**:

| 方案 | Handler 签名 | 说明 | 何时用 |
|---|---|---|---|
| **A. handler 保持同步**(推荐) | `std::function<Response(const Context&)>` | **挂起只发生在框架层**(读 body、发响应在 `Router` 里 `co_await`);handler 内部不挂起 | 现在就走这条 |
| B. handler 也异步 | `std::function<awaitable<Response>(Context)>` | 能装,但协程生命周期更绕 | M2 真要把 DB 丢进 worker pool 时再评估 |

**建议:先走 A。** 别一上来就搞异步 handler —— 那是给现在的自己挖坑。

### 7.2 key 必须带 method

`GET /api/nodes` 和 `POST /api/nodes` 是**两个接口,同一个 path**。只用 path 当 key 直接撞车。

### 7.3 value 是 **struct**,不是裸 `function`

因为一条路由除了"调谁",还要带别的属性 —— 最关键的是**是否需要鉴权**:

```text
Route = { requires_auth , handler }
```

这样 `dispatch` 里能顺手做完鉴权:**命中 → 看 requires_auth → 是则验 token,失败直接 401,根本不进 handler。**
好处:handler 里永远不需要写鉴权代码,也不可能忘写(自动生效,而非靠人记得)。
若 value 只是裸 `function`,你就白白浪费了这个机会,回头还得再改一遍表。

> 将来还能往 `Route` 里加:body 大小上限、是否限流等 —— 都是"路由属性"。

### 7.4 容器与 handler 必须分离

**不要**把 handler 写成 `Router` 类的成员函数。

理由:那样你会从"if/else 函数怪物"变成"**成员函数怪物**" —— 问题没解决,只是从函数搬进了类,而且更糟:
- 所有 handler 被迫共享这个类的成员(DB、配置…),耦合爆炸;
- 加一个 API → 改这个类的头文件 → 所有 include 它的地方全部重编译。

正确做法:

| 角色 | 是什么 | 放哪 |
|---|---|---|
| **Router(容器)** | 只有 `map` + `register()` + `dispatch()`,**不认识任何具体 handler** | `server/http/Router.hpp` |
| **handler(内容)** | 独立函数,按模块分组 | `server/api/NodeApi.hpp`、`UploadApi.hpp`… |
| **register_routes(粘合)** | 把两边连起来,集中列出所有接口 | 一处即可 |

> 原则:**注册表不该认识 handler,是 handler 来注册自己。**

---

## 8. 重构前后对比图

```text
【现在】一个函数套所有业务
   router_handle
    ├─ if (target == "/api/login")    → 匹配 + 鉴权 + 业务 + 响应  全塞一起
    ├─ else if (target == "/api/register") → 匹配 + 鉴权 + 业务 + 响应 全塞一起
    ├─ else if (...)                  → ...
    └─ else                           → 一律回退 index.html(既无 404,也无区分)
   ── 加接口 = 再加一层 else if,函数越来越长 ──

【目标】表驱动分发
   register_routes(router):
       router.post("/api/login",    requires_auth=false, handle_login);
       router.post("/api/register", requires_auth=false, handle_register);
       router.get ("/api/nodes",    requires_auth=true,  handle_list_nodes);
       ...
   ── 加接口 = 写一个 handler + 加一行注册,Router 代码一个字不改 ──
```

---

## 9. 加一个接口的"正确姿势"

**反例(现在):**
1. 打开 `http_server.hpp`
2. 在长链里找地方插 `else if`
3. 在分支里抄一遍"取 body + 拼 resp + 设 Content-Type"
4. 忘了设某个头部 / 忘了鉴权 → 出 bug

**正例(目标):**
1. 在 `server/api/NodeApi.hpp` 新增一个函数 `handle_list_nodes(ctx)`
2. 在 `register_routes` 里加一行注册
3. **不碰 Router 的任何代码**

---

## 10. 落地步骤(渐进式,每步都能跑)

> 和 M0 文档一个原则:**每个小步结束都要能编译、能跑、能演示**,不许推倒重来。

1. **先抽响应** —— 把 `resp["code"]=...` 那套收成统一函数(`ok / 业务失败 / HTTP失败`),并集中 `code` 常量。
   *行为不变,纯减重复,零风险。*
2. **定义 `Context` 与 `Response`** —— 统一 handler 的"一进一出",控制流先不动。
3. **给 handler 定统一签名** —— 按 7.1 选同步版。
4. **建路由表** —— 用 `(method, path)` 作 key,value 用带 `requires_auth` 的 struct。
5. **把现有分支搬成独立 handler** —— `handle_login(ctx)` / `handle_register(ctx)`,行为保持不变(先搬后修)。
6. **接入 `Router::dispatch`** —— 用查表替换 if/else;查不到返回 404(不再回退 `index.html`)。
7. **最后拆文件** —— handler 搬到 `server/api/`,Router 搬到 `server/http/`。

**暂时别做:** 路径参数 `/api/nodes/:id`(精确 map 匹配不了;等 M1 真有需求,再加"按 `/` 切段、逐段比较、遇 `:name` 捕获")。

---

## 11. 边界:不要做什么

| ❌ 别做 | ✅ 只做 |
|---|---|
| 泛型的中间件管道 / 责任链 / 插件系统 | 一张表 |
| 正则路由树 / 自己写路由库 | `map` 精确匹配 + 少量手动分段匹配 |
| 给 repo 写纯虚接口 + 依赖注入 + 工厂 | 统一的 handler 签名 |
| 一次性把 handler 拆成十几个文件 | 统一响应函数 + 统一结果类型 |
| 每个类都拆 `.hpp` / `.cpp` | 一个 `requires_auth` 属性 |

规模判断:总共约 15~20 个接口,**轻量方案足够撑到答辩**,而且更好解释 —— 答辩时一张路由表就是"系统功能清单",一眼看完。

---

## 12. 自检:抽取成功了没有?

问自己一句:

> **"我要加 `/api/nodes` 这个接口,需要改几个文件?"**

- 现在:要改 `http_server.hpp` 里那个大函数(动了框架)→ **没抽出来**。
- 目标:只新增一个 handler + 注册一行,**不改框架** → **抽对了**。

---

## 13. 术语纠正(会影响你想问题的方式)

| 你说的 | 准确的 | 为什么重要 |
|---|---|---|
| "map 作为**工厂**" | **路由表 / 分发表(dispatch table)** | 工厂是"创建对象"的模式;这里是"查表分发"。名字错了,想问题会跑偏 |
| "**函数模板** function" | `std::function`(函数包装器 / 类型擦除) | "函数模板"是 `template<class T> ...`,是另一个东西 |
| "用类成员**避免自由函数**" | 方向反了:应按**模块分文件** | 自由函数本身不是问题,"散落无组织"才是;塞进一个类反而引入隐式 `this`、成员耦合、头文件膨胀。**我们是"拆开",不是"再包一层"** |

---

## 14. 与其它文档的关系

- **M0 步骤 5**(路由 / 静态文件 / SPA fallback):本文是它的"怎么拆"的展开。
- **M0 步骤 6**(鉴权闭环):本文 7.3 的 `requires_auth` 就是它落地的关键。
- **M2**(流式上传 + 线程模型):本文 7.1 的同步/异步决策在那里会被真正用到。
- 目标路径以 [overall-plan.md](overall-plan.md) 的《目标目录结构与关键文件清单》为准。
- 可选:beast / http / asio 的公共别名块(现在 `http_server.hpp` 与已删除的 `api_handle.hpp` 各抄了一份)建议收口到一个公共头(如 `server/http/common.hpp`),避免每个新头重复。

---

## 修订记录

- **2026-10-27**(新建):沉淀 `router_handle` 的分层设计 —— 四层职责、四张图、四个关键决策、
  渐进落地步骤、YAGNI 边界、自检标准与术语纠正。对应 M0 步骤 5/6。
