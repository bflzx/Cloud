# 地基修复(M0)学习型实施计划


>
> 动手前的代码真实状态(文件清单 + 已确认缺陷 + 精确位置)见 overall-plan.md 的《当前代码基线》一节,本文的“现状”描述均以那一节为准。
>
> 步骤 5/6 的“怎么拆”展开见 [路由分层设计](router-architecture.md)(四层职责、路由表、鉴权属性、落地步骤)。

## Context

项目 `/home/bflzx/work/Cloud` 是一个毕业设计网盘:后端 C++20 + Boost.Beast 协程 + MySQL,前端 Vue 3 + Element Plus。日志、连接池、加密、异常等基础组件已经写好,登录/注册也有雏形,但**鉴权没有真正生效、路由结构有缺陷、SQL 存在注入、密钥硬编码**。这些问题会让后续所有业务功能建在不稳的地基上。

本计划只做 M0 地基修复,**以学习为主:只讲改什么、为什么改、涉及哪些知识点、按什么顺序做、如何验证,不提供可直接粘贴的实现代码**。每一步都能独立验收,做完即可继续 M1(目录与文件列表)。

## 范围边界

**本次要做:**
- 配置外置、密钥轮换、仓库卫生、建表 schema
- 编译与头文件卫生(ODR、Makefile 依赖)
- 统一 HTTP 状态码与业务码契约
- SQL 预处理语句、连接池安全
- 路由分类、SPA fallback、静态文件安全、Vite proxy
- JWT 鉴权闭环、前端 401 处理
- 崩溃路径与稳定性收口

**本次明确不做:**
- 上传/下载/分片/断点续传/秒传
- 目录树、重命名、删除、分享、预览
- 流式 body(`buffer_body`/大文件 `file_body`)、Range
- 异步 MySQL、PBKDF2 线程池卸载、多线程 ioc 深度改造
- refresh token、OAuth、RBAC、限流、HTTPS、Docker、CI
- 前端 UI 重构、完整测试框架、压测

## 总体顺序

```text
步骤 1  密钥/配置/schema/仓库卫生
   ↓
步骤 2  编译与头文件卫生
   ↓
步骤 3  错误语义与统一响应
   ↓
步骤 4  SQL 安全与数据库层
   ↓
步骤 5  路由 / 静态文件 / SPA fallback / Vite proxy
   ↓
步骤 6  鉴权闭环与前端联动
   ↓
步骤 7  稳定性收口与完整回归
```

**为什么不能颠倒:**
- 不先做步骤 1,后面每次提交都可能继续带入新密钥、日志、二进制。
- 不先做步骤 2,一旦拆分出多个 `.cpp`,`crypto.hpp`/`json.hpp` 里的非 inline 自由函数会立刻重复定义。
- 不先做步骤 3,鉴权、SQL、路由会各自发明一套错误返回,后面返工三遍。
- 不先做步骤 4,登录本身可被注入,拿它签发的 token 做鉴权测试没有意义。
- 不先做步骤 5,就分不清 API、静态资源、SPA,鉴权中间件会误保护 `/main`,刷新直接 401。
- 步骤 7 必须最后做,否则稳定性测试的对象还在变。

---

## 步骤 1:密钥、配置、schema 与仓库卫生

**目标:** 新环境可复现启动;代码里没有真实凭据;仓库不再跟踪二进制/日志/IDE;数据库有可重复执行的 schema 和唯一约束。

**改动位置:**
- 新建根目录 `.gitignore`
- 新建 `db/schema.sql`(库名沿用 `Cloud`)
- 新建 `server/util/Config.hpp`(需要时配 `.cpp`)
- 修改 `server/main.cpp` 的硬编码 DB 常量与启动流程
- 新建 `.env.example`(只放占位符)和启动说明
- 用 `git rm --cached` 停止跟踪 `server/server`(二进制)、`server/log/*.log`(新增 `server/log/.gitkeep` 保留目录);`web/package-lock.json` 是 lockfile,**保留**跟踪;`docs/` 尚未入库,应纳入版本控制;新建 `.gitignore` 覆盖 `.idea/`、日志、二进制、`node_modules`(根目录游离的 `package.json`(`{}`)/`package-lock.json` 与 `.idea/` 已于 2026-10-27 删除,只需防复发)

**知识点:**
- **配置外置:** 密码、secret、端口、静态目录属于部署配置;环境变量适合本机/容器,但都不应进 git。
- **最小权限:** 应用不应使用 root 连库,应创建只对 `Cloud.*` 有增删改查权限的账号。
- **唯一约束:** 用户名的唯一性由数据库保证;先查再插在并发下仍会插入重复行。
- **Git 索引 ≠ 历史:** `git rm --cached` 只停止跟踪,旧密码仍在历史里,因此**轮换密钥比删文件更重要**。
- **字符集与长度:** salt/hash 是 64 个十六进制字符,列长要匹配;`utf8mb4` 才能存中文和 emoji。

**实施顺序:**
1. 建 `m0-foundation` 分支,记录当前可通接口与已知 bug 作为基线。
2. **先轮换密钥:** 改 DB 密码、生成新 JWT secret、创建专用 DB 账号。先做,因为后面测试不能再用旧密码。
3. 建/核对 schema:确认 `user` 表列类型、字符集、是否有重复用户名。
4. 新建配置读取模块,替换 `main.cpp` 硬编码;缺关键配置时启动失败并打印原因。
5. 加 `.gitignore`,`git rm --cached` 停止跟踪,但**不删本地文件**。
6. 写 `.env.example` 和启动说明,只放占位符。

**验收:**
- `git ls-files` 不再出现二进制、日志;`.gitignore` 能拦住 `.idea/`、`node_modules` 等,防止再次入库。
- `git grep` 搜不到真实密码和 JWT secret。
- 不设 `CLOUD_JWT_SECRET` 启动时快速失败并提示缺配置。
- 专用账号能正常增删改查 `Cloud.*`,执行 `DROP TABLE` 被拒绝。
- `SHOW INDEX FROM Cloud.user` 能看到 username 唯一索引。
- 历史提交里仍能看到旧密码是预期现象;确认已轮换即可。

**常见坑:**
- 改 root 密码把自己锁在外面;先保留一个已登录的 mysql 会话。
- `'user'@'localhost'` 与 `'user'@'127.0.0.1'` 是两个账号。
- 已有重复用户名会让唯一索引创建失败,先查重清理。
- salt/hash 列按 32 字符建会截断。
- `.gitignore` 不会自动取消跟踪。
- IDE 运行配置不会继承 shell 的 `export`,要单独配置。
- 不要把 `.env` 提交。

**工时:** 1–2 天。

---

## 步骤 2:编译与头文件卫生

**目标:** `make clean && make` 可重复成功;改头文件能触发重编译;未来新增 `.cpp` 不出现重复符号。

**改动位置:**
- `server/http_server.hpp`、`server/api_handle.hpp` 补 `#pragma once`(空文件可考虑删除)
- `server/util/crypto.hpp`、`server/util/json.hpp`:自由函数改为"声明在头、定义在 `.cpp`",或先加 `inline`
- 新建 `server/util/crypto.cpp`、`json.cpp`
- `server/makefile`:源文件通配、目标文件、依赖文件、警告选项

**知识点:**
- **`#pragma once` 与 ODR:** 前者只防同一翻译单元重复包含;多个 `.cpp` 包含同一头文件时,非 inline 自由函数仍会重复定义。
- **inline / 内部链接:** 头文件里的自由函数要么 `inline`,要么移到 `.cpp`;`static` 能避免报错但会让每个翻译单元各存一份。
- **翻译单元与依赖:** `.cpp` + 它包含的头文件是一个翻译单元;Makefile 必须记录头文件依赖。
- **`-MMD -MP` + `-include`:** 生成 `.d` 依赖文件,改头文件时才会重编。

**实施顺序:**
1. 先补 `#pragma once`(不改变行为,风险最低)。
2. 再拆 `crypto.hpp`/`json.hpp` 的声明与定义。
3. 改 Makefile 为 `.o` + 链接,加依赖生成和 `-Wall -Wextra`。
4. `make clean && make` 验证。

**验收:**
- 干净构建成功。
- `touch` 任一头文件后,`make` 会重编依赖它的目标。
- 临时加第二个 `.cpp` 包含 `util.hpp`,链接无重复定义。
- 登录/注册仍正常。

**常见坑:**
- 把 `.hpp` 直接放进 `$^` 交给 g++,可能生成 `.gch` 或当头文件处理;应先生成 `.o` 再链接。
- 生成了 `.d` 却没 `-include`,等于没做依赖。
- 新增 `.cpp` 忘记加入源文件列表。
- 头文件里放全局 `using namespace`,污染所有包含者。

**工时:** 1–2 天。若 M0 暂不拆分 `.cpp`,可只做 `#pragma once` 和 Makefile 依赖,完整 ODR 拆分推迟到真正新增翻译单元之前。

---

## 步骤 3:错误语义与统一响应

**目标:** 所有接口遵守同一契约;HTTP 状态码表达协议/认证层,业务 `code` 表达业务层;业务异常不再被降级成 5000;响应不泄露 SQL 和内部路径。

**改动位置:**
- `server/http_server.hpp` 的 `router_handle` 与三个 catch
- 新建 `server/http/Response.hpp`,集中生成 JSON 响应(HTTP 层辅助统一放 `server/http/`)
- `server/util/Exception.hpp`:补齐业务错误码;可增加 `AuthException`
- `server/util/json.hpp`:区分"解析失败"和"字段类型不对"
- `server/util/Log.hpp`:内部日志详细,但不记录密码/token

**先定契约表:**

| 场景 | HTTP | 业务 code |
|---|---|---|
| 成功 | 200 | 0 |
| JSON 格式/字段/类型错误 | 400 | 4001 |
| 用户不存在 | 200 | 4002 |
| 密码错误 | 200 | 4003 |
| 用户名重复 | 200 | 4004 |
| 缺少/无效/过期 token | 401 | 4005 |
| 未知 API | 404 | 4006 |
| 方法不允许 | 405 | 4007 |
| 未预期异常/DB/加密故障 | 500 | 5000 |

原则:**业务上可预期的表单错误用 200 + 业务码;认证失效用 401。** 这样"密码输错"不会触发前端全局退出登录。

> 现状提醒:当前代码里 `business_err::BUSINESS_ERR = 4000`,登录失败分支实际用 `4000`(用户不存在)/`4001`(密码错误),注册失败用 `1000`(`Exception.hpp`)。上表是**目标**契约,迁移时按上表重新分配,并清理这些旧码。

**知识点:**
- **状态码与业务码分离:** 前者给 HTTP 客户端/代理/浏览器,后者给前端页面判断。
- **catch 顺序:** 派生类必须在基类前;`catch(BaseException)` 放在 `BusinessException` 前会把业务错误全吞成 500。
- **内容协商:** JSON 响应必须显式设置 `application/json; charset=utf-8`,不能沿用路径猜出的 MIME。
- **信息泄露:** 日志可有 SQL/路径,响应不可以。

**实施顺序:**
1. 先确认契约表。
2. 加统一响应辅助函数,一次设置状态码、Content-Type、body。
3. 调整 catch 顺序与映射。
4. 替换登录/注册里零散的 `resp["code"] = ...`。
5. 最后补字段校验,缺字段返回 400 而不是让 `asString()` 抛异常变 500。

**验收:**
- 非法 JSON → 400 + JSON + 正确 Content-Type。
- 缺字段 → 400 + 4001。
- 密码错误 → 200 + 4003。
- 未知 `/api/xxx` → 404 + JSON。
- 停掉 MySQL → 500 + 通用信息;日志有详情,响应无 SQL。
- 日志中无明文密码、Authorization、完整 token。

**常见坑:**
- 函数开头写死 `res.result(ok)`,异常分支忘记改。
- catch 顺序写反。
- 把 `e.getCode()` 直接当 HTTP 状态码。
- 只改 body 不改 Content-Type。
- catch 里再次抛异常导致响应发不出去。
- `catch(...)` 吞掉 bug 且无上下文日志。

**工时:** 2–3 天。

---

## 步骤 4:SQL 安全与数据库层

**目标:** 登录/注册不再拼接用户输入;注入 payload 不能改变查询语义;重复用户名返回业务错误而非 500;连接池不返回空指针;`Mysql::close()` 不会 double free。

**改动位置:**
- `server/util/Mysql.hpp`:增加参数化执行/查询接口;`close()` 关闭后置空;用 RAII 管理语句和结果集
- `server/util/MysqlPool.hpp`:`create_conn()` 返回值判空;停止/失败时不要返回可解引用的空 guard;`give_back` 处理空连接
- `server/http_server.hpp`:登录 SELECT、注册 INSERT;登录加 `LIMIT 1`;注册处理 MySQL 1062
- `db/schema.sql`:username 唯一索引(步骤 1)
- `server/util/Exception.hpp`:让 DB 异常能携带原生错误码,或增加重复键专用异常

**知识点:**
- **预处理语句原理:** SQL 模板与参数分两次发送,数据库先解析带占位符的语句,再把参数当"值"绑定;引号、注释符、`UNION SELECT` 都不会变成语法。
- **占位符不替代标识符:** 表名、列名、`ORDER BY` 字段不能用参数绑定,只能白名单;本项目的用户输入只应出现在值的位置。
- **两套结果 API:** `mysql_store_result` 不适用于 prepared statement;prepared SELECT 需要 `mysql_stmt_result_metadata`、逐列缓冲区、`mysql_stmt_bind_result`、`mysql_stmt_fetch`。
- **唯一约束是并发下的最终保证:** 先查再插只改善提示,插入时仍要处理 1062。
- **RAII 与幂等:** 连接、语句、结果集析构时释放;`close()` 必须可重复调用。

**实施顺序:**
1. 先修 `close()` 幂等,用 ASan/valgrind 验证。
2. 在 `Mysql` 里加一个**窄接口**,只覆盖登录 SELECT 和注册 INSERT,不要一上来做通用 ORM。
3. 用一条固定 SQL 做小实验,确认列绑定、NULL、中文、emoji 都正常。
4. 修连接池空指针路径和停止时归还逻辑。
5. 确认唯一索引存在并清理重复数据。
6. 改造登录/注册;注册捕获 1062 映射成业务错误。
7. 用注入 payload、长用户名、特殊字符、emoji 回归。

**验收:**
- 用户名/密码含 `'`、`--`、`UNION SELECT`、反斜杠、`%` 时不能绕过登录或读到他人数据。
- 注册注入 payload 后数据库没有额外行。
- 重复注册返回约定业务码,不是 500。
- SELECT 返回列顺序与代码假设一致。
- ASan 跑"借连接 → 异常 → 归还 → 退出"无 double free。
- 池耗尽时请求等待而不崩溃;池停止后借连接有明确错误。

**常见坑:**
- `MYSQL_BIND` 未清零,长度/缓冲区生命周期错误。
- 只给 buffer 不给长度,或长度在 execute 前失效。
- 缓冲区太小出现 `MYSQL_DATA_TRUNCATED` 未处理。
- 忘记 `mysql_stmt_store_result`/`mysql_stmt_close`/`mysql_free_result`。
- 连接字符集不是 `utf8mb4`。
- 只做"先查再插",并发下仍重复。
- 把 1062 当通用 DB 异常返回 500。
- 以为参数化能保护表名/列名。
- 用 `mysql_real_escape_string` 当最终方案(字符集不对仍可能出问题,只能临时止血)。

**工时:** 4–6 天,是 M0 最重的一步。结果绑定最容易卡住,先做单条固定 SQL 实验。

---

## 步骤 5:路由、静态文件、SPA fallback 与 Vite proxy

**目标:** 请求明确分为"API 白名单 / 受保护 API / 静态资源 / SPA";刷新 `/main` 不白屏;`/assets/*` 缺失返回 404;路径遍历无效;dev 模式通过 proxy 联调。

**改动位置:**
- `server/http_server.hpp`:`router_handle` 重写分类与匹配;`read_file` 错误处理、二进制读取、路径安全;`get_mime` 剥离 query、补 MIME
- 可新建 `server/http/Router.hpp`、`StaticFiles.hpp` 拆分职责
- `server/util/Config.hpp`:增加静态根目录配置
- `web/vite.config.js`:增加 `/api` 代理
- `web/src/util/request.js`:`baseURL` 改为相对路径

**请求分类顺序:**
1. (仅当需要 CORS 时)`OPTIONS` 预检
2. 精确匹配 `/api/login`、`/api/register`,并检查 POST
3. 其他 `/api/*` 进入鉴权分支(步骤 6 补校验)
4. 静态资源:仅 GET/HEAD,且路径在静态根内
5. 其他 GET 非 API 路径:返回 `index.html`
6. 其余:404 / 405

**知识点:**
- **target 含 query:** `/assets/x.js?v=1` 必须先剥离 `?` 再做路由和 MIME 判断。
- **路径规范化:** 只替换 `..` 不可靠;应百分号解码、拒绝空字节和 `..` 段、规范化静态根和候选路径,最后用前缀检查确认没有逃出根目录。
- **SPA history fallback:** `createWebHistory` 下 `/login`、`/main` 不是真实文件;后端对"GET + 非 API + 非静态"返回 `index.html`。
- **鉴权只保护 `/api/*`:** 浏览器直接导航到 `/main` 不会带 `Authorization`;若服务端要求 Bearer,刷新必然 401。SPA 的登录保护由前端守卫负责。
- **反向代理:** Vite dev server 把 `/api` 转发到后端,浏览器看到同源请求,因此**dev 模式不需要 CORS**;生产由后端同源托管 `dist`,同样不需要。
- **CORS 预检:** 只有真正跨源才需要;预检必须在鉴权之前处理,并允许 `Authorization`/`Content-Type`。

**实施顺序:**
1. 先定请求分类和匹配顺序。
2. 修静态文件:配置根目录、剥 query、解码、规范化、前缀检查、二进制读取、失败 404。
3. 修 SPA fallback:仅 GET 且非 `/api`、非 `/assets/`。
4. 加 Vite proxy,`baseURL` 改相对路径,先跑 dev。
5. 再跑 `npm run build` + 后端托管的生产模式。
6. 最后决定是否需要 CORS;有 proxy 且生产同源则不做。

**验收:**
- `/`、`/login`、`/main` 返回 200 + HTML。
- 真实 `/assets/*` 返回 200 且 MIME 正确;不存在的返回 404,不能回退 HTML。
- `curl --path-as-is` 测试 `..`、`%2e%2e`、双重编码、反斜杠变体,期望 400/404。
- `/api/unknown` 返回 404 + JSON。
- `GET /api/login` 返回 405 + `Allow: POST`。
- `npm run dev` 登录请求走 `/api` proxy。
- `npm run build` 后后端托管,刷新 `/main` 正常。

**常见坑:**
- `starts_with("/api")` 让 `/apiary` 也被当成 API。
- 静态资源缺失回退 `index.html`,浏览器按 JS 解析报 MIME 错误。
- 把 `/main` 放进鉴权分支。
- 只处理字面量 `..`。
- `read_file` 不检查打开是否成功,返回空串却仍是 200。
- 文本模式读图片/字体。
- 静态根用 `../web/dist` 依赖当前工作目录。
- MIME 判断把 query 算进去。
- proxy target 用 LAN IP 被防火墙挡住,dev 优先 `127.0.0.1:8080`。

**工时:** 2–4 天。

---

## 步骤 6:鉴权闭环与前端联动

**目标:** 除登录/注册外所有 `/api/*` 要求合法 Bearer token;各种无效情况统一 401;合法 token 能取到 uid,后续 handler 不信任客户端传来的 uid;前端能处理 401;密码框掩码。

**改动位置:**
- `server/http_server.hpp`:提取 `Authorization`、校验 `Bearer ` 前缀、验证 token、失败立即 401 返回
- `server/util/crypto.hpp`:把"验证并取 subject"做成清晰接口;无效 token 返回可预期失败,而不是抛一个会被误映射成 500 的异常
- `server/util/Exception.hpp`:无效 token 用 `AuthException` 或统一 401 映射;PBKDF2/OpenSSL 内部错误仍走 500
- `server/util/Config.hpp`:JWT secret、issuer、过期时间从配置读取,secret 不允许代码内默认值
- `web/src/util/request.js`:401 清 token 并跳登录页,避免循环跳转
- `web/src/views/Login/index.vue`、`Register/index.vue`:密码框 `type="password"`
- `web/src/router/index.js`:守卫只判断 token 是否存在,过期交给 API 401

**知识点:**
- **JWT 结构:** header.payload.signature;payload 只是 Base64URL 编码,不是加密,不能放敏感信息。
- **HS256 校验流程:** 先验签,再校验算法白名单、issuer、`exp`/`nbf`/`iat`。
- **Bearer 认证:** `Authorization: Bearer <token>`,scheme 大小写不敏感;不要从 query 读 token,否则会进访问日志。
- **401 与 403:** 401 是"未认证/认证失效",403 是"已认证但无权限";M0 只有 401。
- **无状态鉴权:** 服务端不存 session;secret 轮换会让旧 token 立即失效,这是预期行为。
- **中间件顺序:** 静态/SPA 不鉴权;白名单在鉴权前;CORS 预检在鉴权前;业务 handler 在鉴权后。

**实施顺序:**
1. 定义白名单与鉴权分支,只作用于 `/api/*`。
2. 实现 token 提取与验证,覆盖缺头、空值、缺 Bearer、乱码、过期、错误 issuer/算法。
3. 登录用配置里的 secret 签发 token,确认 payload 有 `sub`、`iss`、`exp`。
4. 加一个最小受保护探针 `/api/me`,返回当前 token 的 `sub`(不是业务功能,只用于验收)。
5. 用 curl 跑完正反用例,再改前端。
6. 前端 401 处理:清 token、回登录页,注意不要在登录页自触发循环。
7. 密码框掩码;只在 `data.token` 存在时写入 localStorage。

**验收:**
- 无 token / 裸 token / 坏 token / 过期 token / 旧 secret token → 401 + 统一 JSON。
- 合法 token → 200,`/api/me` 返回 uid 与登录用户一致。
- 登录/注册无 token 仍可访问。
- `/`、`/login`、`/main`、`/assets/*` 不需要 token,刷新正常。
- 删除 localStorage token 后访问受保护 API,前端回登录页。
- 密码框显示为圆点。

**常见坑:**
- 白名单前缀匹配放行 `/api/login-anything`;应精确匹配路径 + 方法。
- 忘记去掉 `Bearer ` 前缀。
- 把 `jwt_verify` 异常交给 `catch(BaseException)` 变成 500。
- 先 `get_sub` 再验签,等于信任未验证 token。
- 不限制算法。
- secret 为空或过短;启动时应拒绝。
- 把 `/main` 也要求 Bearer。
- 前端拦截器 import router 形成循环依赖;用独立 auth 工具或 `window.location`。
- 401 时无条件跳登录,在登录页又触发一次。
- 旧 token 残留 localStorage,secret 轮换后一直 401。
- 把 token 放 URL 调试,写进 access log。

**工时:** 3–5 天。

---

## 步骤 7:稳定性收口与完整回归

**目标:** 客户端 RST/半包/超时不导致进程退出;端口占用不 CPU 空转;连接池空指针和关闭路径安全;日志能定位问题;明确记录单线程阻塞的限制;完整冒烟通过。

**改动位置:**
- `server/http_server.hpp`:`remote_endpoint(ec)` 并在 ec 非空时不使用 endpoint;读写错误记录日志;`acceptor_handle` 检查 `open/bind/listen` 的 ec;协程顶层异常边界;检查 `buff.clear()` 对 keep-alive/pipelining 的影响
- `server/util/Mysql.hpp`、`MysqlPool.hpp`:幂等 close、空连接判断、停止时归还
- `server/main.cpp`:启动检查日志目录/静态目录/DB;可选 SIGINT 优雅退出
- 新建 `server/README.md` 或 M0 验收清单

**知识点:**
- **error_code 重载:** `remote_endpoint(ec)`、`async_accept(..., ec)` 不抛异常而是写 ec,必须逐点判断。
- **detached 协程与异常:** 异常可能被静默吞掉或传播到 `ioc.run()`;必须有显式 try/catch 和错误日志。
- **acceptor 错误循环:** `open/bind/listen` 失败后 `async_accept` 可能立即错误完成;只 `continue` 会忙等,CPU 100%。
- **RAII 与幂等:** 析构、close、归还可能在异常/正常两条路径执行,必须允许重复调用。
- **阻塞事件循环:** 单线程 `ioc.run()` 中 PBKDF2 10 万次和同步 MySQL 会阻塞其他连接;这是可测量的性能限制,不是正确性 bug。
- **优雅退出顺序:** 先停 acceptor,再等待/取消协程,最后停连接池和日志。

**实施顺序:**
1. 先修崩溃/忙等路径。
2. 再修连接池空指针、close 幂等、停止时归还。
3. 加启动检查。
4. 测量单线程阻塞(登录耗时、并发排队)。
5. 跑完整冒烟 + 客户端断开/端口冲突/并发三类稳定性测试。
6. 更新 README,写明已知限制和 M1 计划。

**验收:**
- `nc` 发半截请求后关闭,或用 `curl --max-time` 打断,进程存活且日志有记录。
- 启动第二个实例占同一端口,明确报错,`top` 无 CPU 空转。
- 100 次并发登录/静态请求不崩溃;观察延迟与排队。
- ASan 跑异常/归还/退出路径无 double free/use-after-free。
- `kill -INT` 能退出并刷盘。
- README 写明单线程 ioc、同步 DB/PBKDF2、同步静态读取是已知限制。

**常见坑:**
- 用了 `remote_endpoint(ec)` 仍在 ec 非空时调 `address()`/`port()`。
- `async_accept` 出错后 `continue` 忙等。
- detached 协程异常逃逸,日志空白。
- `ioc.stop()` 后立即销毁连接池/Logger,协程仍在运行。
- 池停止时仍有借出连接,归还塞回已停止的池。
- 为了"解决阻塞"直接开多线程 ioc 却没审计共享状态。
- `buff.clear()` 清掉 pipelined 请求的剩余字节。
- 只测正常请求。

**工时:** 2–4 天。

---

## 最小改动集

### P0 不做就不算 M0 完成
1. 密钥外置与轮换;专用 DB 账号。
2. 登录/注册参数化查询。
3. 鉴权闭环:无效 token 401 且立即返回;只保护 `/api/*`。
4. 路由分类与 SPA fallback;未知 API 返回 404 JSON。
5. 静态文件路径规范化 + 前缀检查;读取失败 404。
6. 异常映射:`BusinessException` 保留业务码;JSON 错误 400;内部错误 500 不泄露 SQL。
7. 崩溃路径:`remote_endpoint(ec)`、acceptor ec、协程异常边界。

### P1 应该做
- 根目录 `.gitignore`,停止跟踪二进制/日志/IDE。
- username 唯一索引;`Mysql::close()` 置空;`create_conn` 判空。
- Vite proxy + 前端 `baseURL` 相对路径。
- 前端 401 清 token 跳登录;密码框掩码。
- `#pragma once`;Makefile 头文件依赖。
- 启动时检查日志/静态目录。

### P2 可以推迟
- 完整 ODR 拆分(若 M0 不新增翻译单元)。
- 异步静态文件、异步 DB、PBKDF2 线程池卸载。
- 多线程 ioc/strand 审计。
- CORS(有 proxy 且生产同源时不需要)。
- Git 历史重写(轮换密钥是必须的,历史重写可选)。
- 优雅退出、日志轮转、限流、测试框架、压测。

---

## 风险清单

| 风险 | 触发场景 | 影响 | 缓解 |
|---|---|---|---|
| 路由重写打挂静态资源 | 缺失资源回退成 index.html;MIME 带 query | 白屏、JS MIME 报错 | 先列路由矩阵;缺失一律 404;逐扩展名测 Content-Type |
| 鉴权误保护 SPA | `/main` 被放进受保护分支 | 刷新 401 | 鉴权只作用于 `/api/*`;SPA 交给前端守卫 |
| JWT 格式不一致 | 后端要 Bearer,前端发裸 token;旧 token 残留 | 全部 401 | curl 先定契约;统一加前缀;401 清 token |
| secret 轮换踢出用户 | 更换 JWT secret | 旧会话失效 | 预期行为;401 清理并重新登录;文档说明 |
| 历史密钥泄露 | 旧密码/secret 已在提交历史 | 删文件也不安全 | 先轮换;最小权限账号;公开仓库再考虑 filter-repo |
| prepared statement 回归 | 列顺序、缓冲区、NULL、utf8mb4 处理错 | 登录失败/数据截断 | 先做固定 SQL 小实验;准备注入/中文/emoji 用例 |
| 状态码变化破坏前端 | 后端开始返回 401/400,前端只认 200 | 提示网络异常/无限跳转 | 先定契约表;同步改拦截器;密码错误用 200+业务码 |
| schema 迁移失败 | 重复用户名/列长不足 | 唯一索引建不上 | 先备份查重;salt/hash 用 64 字符 |
| 静态根依赖 CWD | 从别处启动 | 首页 404 | 配置绝对路径;启动时检查 |
| MySQL 8.4 认证插件 | 旧客户端不支持 caching_sha2_password | 专用账号连不上 | 核对 libmysqlclient;必要时调整认证方式 |
| 单线程阻塞被误判 | PBKDF2/DB 慢 | 并发体验差 | M0 测量记录;不盲目开多线程 |
| 日志泄露敏感信息 | 记录 Authorization/密码/token | 二次泄露 | 访问日志只记方法/路径/UA |
| 构建产物过期 | 改了前端没 build | 看起来像后端 bug | 验收前固定 `npm run build` |

---

## 学习节奏与自测

### 建议节奏(可按实际调整)

| 周 | 内容 | 产出 |
|---|---|---|
| 第 1 周 | 步骤 1 + 2 | 可复现启动、密钥外置、schema、干净构建 |
| 第 2 周 | 步骤 3 | 统一响应契约、异常映射 |
| 第 3–4 周 | 步骤 4 | 参数化登录/注册、唯一约束、连接池安全 |
| 第 5 周 | 步骤 5 | SPA fallback、静态安全、Vite proxy |
| 第 6–7 周 | 步骤 6 | JWT 鉴权闭环、前端 401、密码框 |
| 第 8 周 | 步骤 7 | 稳定性测试、完整冒烟、README |
| 第 9–12 周 | 缓冲 | 代码复盘、答辩演示、可选性能学习 |

### 每日
- 先花 1–2 小时读本步骤知识点,写 5 行笔记:改什么、为什么、怎么验证、可能坏什么、下一步。
- 只做一个小的垂直切片,完成即验证,不要一天跨三个步骤。
- 每天结束前 `git diff` 自查:有没有密钥、日志、二进制、临时文件。
- 每步结束打 tag(如 `m0-step4`),可回退。

### 最小冒烟清单

| 检查 | 期望 |
|---|---|
| `make clean && make` | 成功,无重复符号 |
| 不设 `CLOUD_JWT_SECRET` 启动 | 快速失败,提示缺配置 |
| `/`、`/login`、`/main` | 200 + HTML |
| 真实 `/assets/*` | 200,MIME 正确 |
| 不存在的 `/assets/*` | 404,不是 index.html |
| `curl --path-as-is` 路径遍历 | 400/404,读不到文件 |
| `/api/nope` | 404 + JSON |
| 正常登录 | 200,code=0,有 token |
| 错误密码 | 200 + 约定业务码 |
| 注入 payload 登录 | 不成功,不返回他人数据 |
| 重复注册 | 业务错误,不是 500 |
| 无 token 访问 `/api/me` | 401 + JSON |
| 坏 token / 过期 token | 401 |
| 合法 token 访问 `/api/me` | 200,uid 正确 |
| `npm run dev` 登录 | 走 `/api` proxy 成功 |
| 删除 token 后访问受保护页 | 清 token,回登录页 |
| 半包/超时断开 | 服务存活,日志有记录 |
| 第二个实例占端口 | 明确报错,CPU 不空转 |
| `git ls-files` / `git status` | 无二进制/日志/IDE/密钥 |
| 日志 | 有访问和错误记录,无 token/密码 |

---

## M0 完成定义

- 新机器按 README 设置环境变量即可启动,不改源码。
- 仓库无真实密钥;旧密钥已轮换。
- schema 可重复执行,username 唯一。
- 登录/注册参数化,注入用例失败但服务正常。
- 鉴权只保护 `/api/*`,401 语义正确,前端能处理。
- `/main` 刷新、静态资源、未知 API、路径遍历行为符合预期。
- 客户端断开、端口冲突、并发请求不崩溃、不空转。
- 每步有 commit/tag,可回退。
- README 写清已知限制:单线程、同步 DB/PBKDF2、同步静态读取、CORS 策略、M1 计划。

## 后续里程碑(仅说明边界,本次不做)

完成 M0 后再进入:目录与文件列表(M1)→ 流式上传下载(M2)→ 分片/断点续传/秒传(M3)→ 前端文件管理界面(M4)→ 分享(M5)→ 预览(M6)→ 工程化收尾(M7)。本次地基修复是这些功能的前提,但不在本次实施范围内。

---

## 修订记录

- **2026-10-26**(按仓库真实状态核对后修订):
  - schema 路径统一为 `db/schema.sql`(原步骤 1/4 写作 `server/sql/schema.sql`);
  - 配置模块统一为 `server/util/Config.hpp`(原步骤 6 写作 `server/config.hpp`);
  - 响应模块统一为 `server/http/Response.hpp`(原写作 `server/util/Response.hpp`);
  - 步骤 1 依据 `git ls-files` 实际结果修正“停止跟踪”清单,并订正 `.idea/` 的表述(实际未被跟踪);
  - 步骤 3 补充现有 `4000`/`4001`/`1000` 业务码说明,明确目标契约需重新分配;
  - 新增指向 overall-plan.md《当前代码基线》的引用。

- **2026-10-27**(同步仓库清理):根目录游离的 `package.json`(`{}`)/`package-lock.json` 与 `.idea/` 已删除;步骤 1 的“停止跟踪”清单据此更新,并把 `.idea/` 从“需取消跟踪”改为“已被删除,靠 `.gitignore` 防复发”;验收标准同步调整。

- **2026-10-27**(新增文档):新建 [路由分层设计](router-architecture.md),并在 Context 加入引用。

- **2026-10-27**(修正):`web/package-lock.json` 应**保留**跟踪;此前误列入“停止跟踪”清单,已更正。并补充 `server/log/.gitkeep`。
