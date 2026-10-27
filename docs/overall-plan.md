# 网盘毕业设计 MVP 整体实施方案

## Context

这是一个本科毕业设计网盘项目:后端 C++20 + Boost.Beast 协程 + MySQL,前端 Vue 3 + Element Plus。当前已完成日志、连接池、加密、异常等基础设施,以及登录/注册雏形,但鉴权尚未真正生效、路由存在结构性缺陷、全链路使用 `string_body` 无法传大文件。

用户有 3 个月以上时间,目标是先交付可演示的 MVP,再逐步补齐亮点。最终功能范围:登录注册、多级目录、流式上传/下载、分片上传 + 断点续传 + 秒传、删除/重命名、分享链接、在线预览、多用户隔离。

**总体策略:每个里程碑结束时都必须有一个能跑、能演示的版本。** M0–M2 完成时即为可答辩的最小 MVP;之后每个里程碑都是在此基础上的增量亮点。

> 本文是整体路线图。M0 的详细学习型步骤、知识点、验收方法和常见坑,见同目录的 [地基修复计划](foundation-plan.md)。
> 路由/分层的拆解思路与图谱见 [路由分层设计](router-architecture.md)。

---

## 当前代码基线(2026-10-26 核对)

> 动手前的真实状态,后续章节的“现状”描述以此为准。核对方式:`git ls-files` + 逐文件阅读 `server/`、`web/src/`。

**已存在**
- 后端 `server/`:`http_server.hpp`(约 270 行,一个类里同时承担 acceptor、连接循环、if/else 路由、静态托管)、`main.cpp`(35 行)、`makefile`(6 行,`g++ $^`);`util/` 下 `Log.hpp`、`Mysql.hpp`、`MysqlPool.hpp`、`ThreadPool.hpp`、`crypto.hpp`、`json.hpp`、`Exception.hpp`、`SingletonBase.hpp`;`api_handle.hpp` 为空壳(仅 `namespace api {}`)。
- 前端 `web/`:Vue 3 + Element Plus + vue-router;`views/` 有 `Login`、`Register`、`Main`(占位页),`util/request.js`;`components/HelloWorld.vue` 与 `assets/{hero.png,vue.svg,vite.svg}` 属模板残留;`dist/` 已有构建产物,后端从 `../web/dist` 读取静态文件。

**已确认缺陷(括号内为对应 M0 步骤)**
- 凭据硬编码:`main.cpp` 中 DB 用 `root` + 明文密码,连 `192.168.195.129:3306`;`http_server.hpp` 中 JWT `secret` 明文写死、`expire_sec=3600`(步骤 1)。
- 登录 `SELECT`、注册 `INSERT` 均字符串拼接,存在注入(步骤 4)。
- 鉴权是空壳:`http_server.hpp` 标着 `//TODO 鉴权`,只判断 `Authorization` 头“是否存在”,**从不验签**;白名单也不含 SPA 路径(步骤 5、6)。
- 路由为单条 if/else 链:未匹配路径一律回退 `index.html`,既无未知 API 的 404,也无静态缺失的 404(步骤 5)。
- `read_file` 不判打开失败;`get_mime` 不剥离 query(步骤 5)。
- `Mysql::close()` 关闭不置空(重复调用 double free);`MysqlPool::borrow()` 在停止/失败时可能返回可解引用的空 guard;`create_conn()` 未判空(步骤 4)。
- `remote_endpoint()` 未用带 `ec` 的重载;`acceptor_handle()` 完全忽略 `open/bind/listen` 的 `ec`,失败会忙等(步骤 7)。
- `http_server.hpp`、`api_handle.hpp` 缺 `#pragma once`;`makefile` 把 `.hpp` 当源文件交给 g++;`crypto.hpp`/`json.hpp` 的自由函数非 `inline`(拆 `.cpp` 即重复定义)(步骤 2)。
- `crypto::jwt_verify` 失败时抛异常而非返回 `false`,`router_handle` 的 catch 顺序会把业务异常降级为 `code=5000`(步骤 3、6)。
- 仓库卫生:无 `.gitignore`;已跟踪 `server/server`、`server/log/*.log`、`web/package-lock.json`;`docs/` 未入库(步骤 1)。(根目录游离的 `package.json`(`{}`)/`package-lock.json` 与 `.idea/` 已于 2026-10-27 删除。)
- 前端:登录/注册密码框未加 `type="password"`;`request.js` 写死 `baseURL=http://192.168.195.129:8080/api`;无 Vite dev proxy;`Main/index.vue` 仅占位(步骤 5、6)。

---

## 里程碑路线图

| 里程碑 | 产出(可演示) | 预估工时 |
|---|---|---|
| **M0 地基修复** | 登录/注册真正可用;鉴权生效;刷新页面不再白屏;SQL 注入修复;配置外置 | 12–16 h(学习型 15–26 天,见地基文档) |
| **M1 目录与文件列表** | 建表;多级目录;列表/新建文件夹/重命名/删除;用户隔离 | 16–20 h |
| **M2 流式上传下载** | 单文件上传下载;大文件不 OOM;Range 下载;线程模型改造 | 20–28 h |
| **M3 分片上传** | 分片、断点续传、秒传、哈希校验 | 24–32 h |
| **M4 前端文件管理界面** | 完整文件管理页(列表/面包屑/拖拽上传/进度/右键菜单) | 30–40 h |
| **M5 分享链接** | 文件/目录分享;密码;有效期;次数限制 | 12–16 h |
| **M6 在线预览** | 图片/txt/PDF/音视频预览;预览 token | 10–14 h |
| **M7 工程化收尾** | 测试、压测、日志轮转、优雅退出、文档、演示脚本 | 20–30 h |
| **合计** | | **144–196 h** |

> **最小 MVP 截止线:M0 + M1 + M2 + M4 的基础部分 ≈ 70–90 h。** 建议先冲到这条线,确保有可交付成果,再做 M3/M5/M6。

---

## M0:地基修复

> 详细计划见 [foundation-plan.md](foundation-plan.md)。这里只列范围和顺序。

**目标:** 让项目从"能演示"变成"能继续长出业务功能"。

1. **配置与仓库卫生**:DB 密码、JWT secret 外置到环境变量;轮换已泄露凭据;创建最小权限 DB 账号;根目录 `.gitignore`;停止跟踪二进制和日志;补建表 schema。
2. **编译卫生**:补 `#pragma once`;头文件自由函数改 `inline` 或移到 `.cpp`;Makefile 增加头文件依赖。
3. **错误语义**:HTTP 状态码与业务码分离;统一 JSON 响应;修正 catch 顺序,业务异常不再降级成 5000。
4. **SQL 安全**:登录/注册改用 MySQL 预处理语句;`Mysql::close()` 幂等;连接池空指针检查。
5. **路由与静态资源**:把 if/else 链改成明确分类(API 白名单 / 受保护 API / 静态资源 / SPA fallback);路径规范化防遍历;Vite dev proxy。
6. **鉴权闭环**:只保护 `/api/*`;校验 Bearer JWT;失败立即 401;前端 401 清 token 回登录页。
7. **稳定性**:`remote_endpoint(ec)`、acceptor 错误码检查、协程异常边界;记录单线程阻塞的已知限制。

**M0 验收:**
- 错误密码/注入 payload 无法登录;无 token 访问受保护 API 返回 401。
- 带 token 刷新 `/main` 显示页面而不是白屏。
- 路径遍历 payload 无法读取静态根之外的文件。
- 仓库中不再跟踪二进制、日志和 IDE 目录,源码中无真实密钥。

---

## M1:数据库与目录/文件列表

### 1.1 建表脚本
新建 `db/schema.sql`(库名沿用 `Cloud`),核心表:

- **`user`**:id、username(唯一)、password、salt、status、quota/used、时间戳。
- **`file_blob`**:id、content_hash(唯一,SHA-256)、size、storage_key、ref_count。支持内容去重和秒传。
- **`file_node`**:id、owner_id、parent_id(0 = 根)、name、is_dir、blob_id、size、mime、deleted_at、deleted_seq。
  - `UNIQUE(owner_id, parent_id, name, deleted_seq)`:活跃行 `deleted_seq=0`;软删除时置为自身 id,使删除后可以重建同名节点,且多个已删除同名节点不冲突。
  - 所有查询必须带 `owner_id`,这是用户隔离的核心。
- **`share`**:id、share_token(唯一)、owner_id、node_id、密码哈希/盐、expire_at、max_downloads、download_count、status。
- **`upload_session`**:id、upload_id(唯一)、owner_id、parent_id、filename、total_size、chunk_size、total_chunks、content_hash、status、result_node_id、expires_at。
- **`upload_chunk`**:id、session_id、chunk_index、chunk_size、chunk_hash;`UNIQUE(session_id, chunk_index)`。

### 1.2 仓储与服务层
新建:
- `server/repo/NodeRepo.hpp`:`list_children/create_node/find_by_id/rename/move/soft_delete/descendant_check`。
- `server/repo/UserRepo.hpp`:`find_by_username/create/find_by_id`。
- `server/service/FileService.hpp`:校验父目录归属、同名校验(409)、目录非空删除策略、禁止移动到自身子目录。
- `server/api/NodeApi.hpp`:`GET /api/nodes?parent_id=`、`POST /api/nodes/folder`、`PATCH /api/nodes/:id`、`DELETE /api/nodes/:id`。

### 1.3 前端
新建 `web/src/views/Drive/index.vue`、`web/src/api/node.js`、`web/src/components/Breadcrumb.vue`。路由 `/main` 指向 Drive 页。

### M1 验收
- 用户 B 用自己 token 访问 A 的 node id 返回 404。
- 同目录重名创建返回 409。
- 删除目录后列表不可见;重建同名目录成功。
- 移动目录到自己的子目录被拒绝。

---

## M2:流式上传/下载 + 线程模型(技术关键点)

**先做"一个请求传一个文件",不要直接上分片。** 这一步同时完成线程模型改造。

### 2.1 请求处理改为 header-first
修改 `server/http_server.hpp` 的 `client_handle`:
- 用 `http::request_parser<http::buffer_body>` 先 `async_read_header`。
- 路由匹配和鉴权完成后再按 `BodyMode` 读 body。
- 超限返回 413 并 `Connection: close`;支持 `Expect: 100-continue`。

新建 `server/http/StreamBody.hpp`:
- `stream_body_to_temp(stream, parser, path, max_bytes)`:`async_read_some` + `pwrite` 分块落盘,不在内存累积。
- `read_json_body(...)`:JSON 接口上限 256 KB。

新建 `server/http/Response.hpp`:
- `send_json/send_error/send_file/send_range`;统一 CORS 与安全头。
- 整文件用 `http::response<http::file_body>`;Range 用 `buffer_body`,返回 206 + `Content-Range`,`Accept-Ranges: bytes`,非法 Range 返回 416。
- `HEAD` 只发头。下载前 `stream.expires_never()`。

### 2.2 线程模型(必须遵守,否则会死锁)
现状:单线程 `ioc.run()` + 协程内同步 MySQL `borrow()`(池空时 `cv_.wait`)+ PBKDF2 10 万次迭代。
修改 `server/main.cpp`:io 线程 8 个、独立 `asio::thread_pool` 用于 DB/哈希/合并、MySQL 池容量 8。

四条硬规则:
1. **持有 `MysqlConnGuard` 时绝不 `co_await`**;DB 操作放进花括号作用域,离开即归还。
2. io 线程不直接 `borrow()`;DB 工作通过 `asio::post(worker_pool, use_awaitable)` 投递。
3. `MysqlPool::borrow()` 增加超时(新增 `borrow_for(3s)`),超时返回空 guard → 503,避免无限等待。
4. 每个请求最多借一次连接;事务内不做文件 IO。

同时修复:
- `server/util/MysqlPool.hpp` 对 `create_conn()` 返回值判空。
- `server/util/Mysql.hpp` 的 `close()` 置空 `mysql_`,避免 double free。
- `server/http_server.hpp` 的 `remote_endpoint()` 改用带 `ec` 的重载;`client_handle` 外层 try-catch,确保异常不穿透 `detached` 协程终止进程。
- `acceptor_handle` 检查 `open/bind/listen/accept` 的 `ec`,失败时记录 FATAL 并退出,避免 CPU 100% 忙等。

### 2.3 上传与下载
- `server/service/Storage.hpp`:`blob_path(hash)`、`temp_dir(uid,upload_id)`、`atomic_rename`;物理路径只由服务端生成,绝不拼接用户文件名。
- `server/util/Hash.hpp`:`sha256_file(path)` 分块 EVP,避免整文件读入内存;`sha256_hex(string_view)`。
- `server/api/UploadApi.hpp`:`handle_upload_simple`(落临时文件 → 计算 SHA-256 → 建 blob + node)。
- `server/api/FileApi.hpp`:`handle_download`;`Content-Disposition` 中文文件名用 RFC 5987。

### M2 验收
- 上传 200 MB 文件,服务端 RSS 峰值不超过约 200 MB;`sha256sum` 一致。
- 下载后原文件 `cmp` 一致,中文文件名正确。
- 上传中 `kill -9` 客户端,服务端继续正常响应其他请求。
- 10 并发上传/下载不崩溃,连接全部归还。
- `curl -r 0-99` 返回 206 且字节正确。

---

## M3:分片上传、断点续传、秒传

### 协议
```text
POST /api/upload/init   { parent_id, filename, file_size, content_hash, chunk_size }
  → 秒传 { instant:true, node } | { upload_id, chunk_size, total_chunks, uploaded:[] }
GET  /api/upload/status?upload_id=...
  → { status, total_chunks, uploaded:[...] }
POST /api/upload/chunk?upload_id=...&index=3   (application/octet-stream, 原始字节)
POST /api/upload/merge  { upload_id }          → { node }
DELETE /api/upload/:upload_id                  取消
```

### 实现要点
- 默认分片 4 MB;路径 `storage/tmp/{uid}/{upload_id}/{index}.part`;先写 `.tmp` 再 `rename`,最后写 DB;DB 失败删除文件。
- 断点续传:`init` 按 `(owner_id, content_hash, parent_id, status=0)` 查已有会话并返回 `uploaded`;`status` 与磁盘核对,清理"DB 有、磁盘无"的记录;前端 localStorage 缓存 `upload_id` 作为加速。
- 秒传:`init` 先查 `file_blob.content_hash`,存在则事务内 `ref_count+1` 并插入 `file_node`,返回 `instant:true`。
- 合并:条件更新 `UPDATE upload_session SET status=1 WHERE id=? AND owner_id=? AND status=0` 抢占;在 worker pool 中按序流式拼接并同时计算 SHA-256;校验通过后 `rename` 到 blob 路径;事务内 blob upsert + `ref_count+1` + 插入 node;提交后删除分片。重复 merge 返回同一 `result_node_id`(幂等)。
- 并发:前端 3 个分片并发,单分片最多重试 3 次(指数退避);分片上传幂等;并发 merge 只有一个成功,其余返回 409。
- 前端:`web/src/util/uploader.js`(`UploadTask/UploadQueue`)、`web/src/util/sha256.js` + Web Worker(`hash-wasm`,增量哈希)、`web/src/components/UploadPanel.vue`。若 `hash-wasm` 无法安装,退化方案:保留断点续传,取消秒传(按 `size+name+lastModified` 恢复会话)。

### M3 验收
- 1 GB 文件、4 MB 分片上传成功且 SHA-256 一致。
- 上传 40% 暂停、刷新页面、继续,已完成分片不重传。
- 上传中 `kill -9` 服务端,重启后继续并合并成功。
- 同一文件第二次上传秒传。
- 篡改分片后 merge 返回 400;并发 merge 只有一个成功。

---

## M4:前端文件管理界面

- `web/src/views/Drive/index.vue`:文件表格/网格、排序、多选、右键菜单、空状态。
- `web/src/components/Breadcrumb.vue`:面包屑 + 目录树导航。
- `web/src/components/NodeTable.vue`、`NodeContextMenu.vue`、`RenameDialog.vue`、`MoveDialog.vue`。
- `web/src/components/UploadPanel.vue`:拖拽上传、队列、进度/速度、暂停/继续/取消、失败重试。
- `web/src/components/ShareDialog.vue`(M5 使用)。
- `web/src/api/node.js`、`upload.js`、`share.js`。
- 路由:`/main`(受保护)、`/s/:token`(公开分享页)。
- 401 统一由 axios 拦截器处理:清 token、跳登录。
- 删除 `web/src/components/HelloWorld.vue`、`from` 依赖等模板残留。

### M4 验收
- 拖拽上传多个文件能显示进度;取消后服务端有清理。
- 刷新页面保持当前目录;浏览器前进/后退正常。
- 移动端/窄屏不崩(不要求精致)。

---

## M5:分享链接

- `server/repo/ShareRepo.hpp`、`server/service/ShareService.hpp`、`server/api/ShareApi.hpp`。
- 新建分享:`POST /api/shares`(node_id、密码、有效期、次数)→ 返回 `/s/{token}`。
- 公开接口:`GET /api/share/:token/info`、`POST /api/share/:token/verify`、`GET /api/share/:token/list`、`GET /api/share/:token/download`。
- 密码用现有 `crypto::getSalt` + `pbkdf2_hash`;校验成功签发短时(2 h)share JWT,claims 含 `scope=share`、`share_id`、`node_id`。
- 目录分享越权防线:节点沿 `parent_id` 上溯(最多 256 层)必须经过 `share.node_id`,且 `owner_id` 匹配、`deleted_at IS NULL`。
- 下载次数用条件 `UPDATE` 原子递增;超过次数返回 410。
- 分享 JWT 只能访问 share 路由,不能访问普通用户 API。
- 前端 `web/src/views/Share/index.vue`:无需登录,支持密码、列表、预览、下载。

### M5 验收
- 文件/目录分享在无痕窗口可直接访问并下载。
- 密码错误 401,正确后获得 access token。
- 过期/撤销/超次数返回 404 或 410。
- 构造分享目录之外的 `folder_id` 返回 404。

---

## M6:在线预览

- `GET /api/nodes/:id/preview-token` 返回 5 分钟只读 JWT(`scope=preview`),内容接口接受 `?access_token=`。
- 图片/txt/PDF/音视频白名单类型 `Content-Disposition: inline`;其他一律 `attachment`。
- 用户上传的 `.html/.svg/.js` 强制 `attachment` 且 `X-Content-Type-Options: nosniff`,防止同源存储型 XSS 读取 localStorage token。
- 视频/音频依赖 M2 的 Range 实现。
- 前端 `web/src/components/PreviewDialog.vue`:图片 `<img>`、文本 `<pre>`(上限 1 MB)、PDF `<iframe>`、音视频 `<video>/<audio>`;Office 显示"请下载"。

### M6 验收
- 图片/txt/PDF/mp4 可预览,视频拖动进度条发出 206。
- 上传 `x.html` 后访问内容接口触发下载而非执行脚本。
- 过期/非法 preview token 返回 401。

---

## M7:工程化收尾

- **测试**:
  - `tests/curl_smoke.sh`:登录 → 建目录 → 上传 → 列表 → 下载 → 分享 → 删除全链路。
  - `tests/concurrency.sh`:50 并发列表/下载。
  - 边界用例:空文件、超大文件名、中文名、同名冲突、越权访问、非法 Range、超大 body。
  - ASan 构建跑一遍完整流程。
- **限流**:登录/分享密码按 IP + 用户名 5 次/分钟,超限 429。
- **日志轮转**:`server/util/Log.hpp` 增加 `rotate_if_needed()`(按 50 MB 切分,保留 10 个);access log 同样处理。
- **优雅退出**:`signal_set` 捕获 SIGINT/SIGTERM,关闭 acceptor,等待在途请求或超时,再停连接池。
- **构建**:把 `server/makefile` 加头文件依赖(`-MMD -MP`)或改用 CMake;增加 `-O2` 和 ASan 目标;头文件中的非模板函数加 `inline`。
- **文档**:`README.md`(启动步骤)、`docs/API.md`、`docs/demo.md`(10 分钟答辩脚本)、`db/schema.sql`。
- **清理**:删除无用文件、确认 `.gitignore` 生效、确认无凭据入库。

### M7 验收
- 全流程无 500;所有错误输入返回明确 4xx。
- 50 并发下载/列表 5 分钟无崩溃、无连接泄漏。
- 日志按大小轮转;重启后数据一致。
- 新环境按 README 可一次性编译、建库、启动。

---

## 目标目录结构与关键文件清单

> 两份文档出现的路径以本节为准;新增模块统一遵守以下分层。

```text
server/
├── main.cpp                  # 启动:配置读取、多线程 ioc + worker pool、优雅退出
├── makefile                  # .o + 依赖(-MMD -MP)、-Wall -Wextra、-O2/ASan/测试目标
├── http_server.hpp           # 现有:接入 Router / StreamBody,补异常与 ec 处理
├── util/                     # 无业务依赖的通用组件
│   ├── Config.hpp  Hash.hpp            # 新增
│   ├── Log.hpp  Mysql.hpp  MysqlPool.hpp  ThreadPool.hpp
│   └── crypto.hpp  json.hpp  Exception.hpp  SingletonBase.hpp
├── http/                     # 新增:HTTP 层
│   ├── Router.hpp  StaticFiles.hpp
│   ├── StreamBody.hpp        # M2
│   └── Response.hpp
├── api/                      # 新增:请求处理
│   ├── Auth.hpp
│   └── NodeApi.hpp  UploadApi.hpp  FileApi.hpp  ShareApi.hpp
├── repo/                     # 新增:数据访问
│   └── NodeRepo.hpp  UserRepo.hpp  ShareRepo.hpp  UploadRepo.hpp
└── service/                  # 新增:业务逻辑
    └── FileService.hpp  Storage.hpp  UploadService.hpp  ShareService.hpp

db/schema.sql                 # 建表脚本(库名 Cloud)
tests/curl_smoke.sh  tests/concurrency.sh
web/src/{api,components,util,views}/...    # 见 M1 / M4 / M5 / M6
```

**新增**
- `server/util/Config.hpp`、`server/util/Hash.hpp`
- `server/http/Router.hpp`、`StaticFiles.hpp`、`StreamBody.hpp`、`Response.hpp`
- `server/api/Auth.hpp`、`NodeApi.hpp`、`UploadApi.hpp`、`FileApi.hpp`、`ShareApi.hpp`
- `server/repo/NodeRepo.hpp`、`UserRepo.hpp`、`ShareRepo.hpp`、`UploadRepo.hpp`
- `server/service/FileService.hpp`、`Storage.hpp`、`UploadService.hpp`、`ShareService.hpp`
- `db/schema.sql`
- `tests/curl_smoke.sh`、`tests/concurrency.sh`
- `web/src/views/Drive/index.vue`、`web/src/views/Share/index.vue`
- `web/src/components/`(Breadcrumb、NodeTable、NodeContextMenu、RenameDialog、MoveDialog、UploadPanel、PreviewDialog、ShareDialog 等)
- `web/src/api/node.js`、`upload.js`、`share.js`
- `web/src/util/uploader.js`、`sha256.js`

**修改**
- `server/http_server.hpp`:header-first 解析、接入 Router、流式读写、异常与 `ec` 处理
- `server/main.cpp`:配置外置、多线程 io_context、worker pool、优雅退出
- `server/util/Mysql.hpp`:`exec_prepared/query_prepared`、`close()` 置空
- `server/util/MysqlPool.hpp`:带超时 borrow(`borrow_for`)、空指针检查
- `server/util/Exception.hpp`:错误码/HTTP 状态映射、`AuthException`
- `server/util/crypto.hpp`:自由函数 `inline` 化或拆 `.cpp`;增加 `random_hex`、share/preview token
- `server/util/json.hpp`:自由函数 `inline` 化或拆 `.cpp`
- `server/util/Log.hpp`:轮转
- `server/makefile`:`.o` + 依赖、优化、测试目标
- `web/src/util/request.js`:`baseURL` 相对 `/api`、401 跳转
- `web/vite.config.js`:dev proxy
- `web/src/router/index.js`:Drive、Share 路由
- `web/src/views/Login/index.vue`、`Register/index.vue`:密码框 `type="password"`、错误提示

**删除/清理**
- 移出版本控制:`server/server`(二进制)、`server/log/*.log`,并新增根目录 `.gitignore`;补 `server/log/.gitkeep` 保留日志目录(注:`web/package-lock.json` 是前端 lockfile,**应保留**在版本控制中)
- 删除:`server/api_handle.hpp`(空壳)、`web/src/components/HelloWorld.vue`、`web/src/assets/hero.png`+`vue.svg`+`vite.svg`、`web/package.json` 中的 `from` 依赖(根目录游离的 `package.json`/`package-lock.json` 已于 2026-10-27 删除)
- 纳入版本控制:`docs/`、`.env.example`

---

## 全局验证方案

按里程碑逐项执行,最终完整验收:

1. **冷启动验证**:清空数据库和 `storage/`,按 README 建库、编译、启动。
2. **功能冒烟**:`bash tests/curl_smoke.sh`,覆盖登录 → 建目录 → 上传(小/大文件)→ 列表 → Range 下载 → 分片续传 → 秒传 → 分享 → 预览 → 删除。
3. **隔离验证**:创建 A、B 两个用户,交叉访问对方 node_id、share token。
4. **异常验证**:错误 token、过期 token、越权 id、非法 Range、超大 body、同名校验、路径遍历 payload。
5. **并发/稳定性**:`tests/concurrency.sh` 50 并发 5 分钟;上传中 kill 客户端;重启服务后断点续传。
6. **内存验证**:ASan 构建跑完整流程,确认无泄漏/double free。
7. **答辩演示**:按 `docs/demo.md` 在 10 分钟内完成注册 → 登录 → 上传(展示进度/暂停/续传)→ 目录管理 → 分享 → 预览 → 多用户隔离展示。

---

## 风险与取舍

| 风险 | 应对 |
|---|---|
| 分片/秒传耗时超预期 | M2 完成即达到可答辩 MVP;M3 可只保留断点续传,砍秒传 |
| 线程模型改造引入死锁 | 严格遵守"持锁不 co_await"四条规则;borrow 加超时;M2 单独压测 |
| 前端工作量最大 | M4 先做列表/上传/下载主路径,右键菜单和批量操作可后补 |
| 在线预览安全 | 一律 `nosniff` + 非白名单 `attachment`;预览 token 短时效只读 |
| 跨用户秒传的隐私争议 | 默认全局去重并在论文中说明;如导师关注,改为 `(owner_id, content_hash)` 去重 |
| 凭据已泄露 | M0 必须轮换 DB 密码和 JWT secret,并清理 git 跟踪 |
| 答辩必问点 | 用户隔离、SQL 预处理、JWT 校验、大文件流式处理、分片与断点续传、并发安全 |

**可砍功能(时间不足时)**:秒传 → 目录分享(只做文件分享)→ PDF/视频预览(只做图片和文本)→ 配额统计 → 回收站恢复。

---

## 修订记录

- **2026-10-26**(按仓库真实状态核对后修订):
  - 新增《当前代码基线》一节,记录动手前文件清单与已确认缺陷(含精确位置),作为后续“现状”描述的唯一依据;
  - 将《关键文件清单》升级为《目标目录结构与关键文件清单》,统一分层(util/http/api/repo/service)与新增路径;
  - 修正路径矛盾:`server/config.hpp` → `server/util/Config.hpp`;`server/api/Response.hpp` → `server/http/Response.hpp`;schema 统一为 `db/schema.sql`;
  - 补充清理项:根目录游离 `package.json`/`package-lock.json`、空壳 `server/api_handle.hpp`、`web/src/assets` 模板残留、`web/package.json` 的 `from` 依赖;并注明 `docs/` 需入库。

- **2026-10-27**(同步仓库清理):根目录游离的 `package.json`(`{}`)/`package-lock.json` 与 `.idea/` 已删除,基线章节与清理清单同步更新;`.gitignore` 仍待补上(用于防止 `.idea/`、日志、二进制、`node_modules` 再次入库)。

- **2026-10-27**(新增文档):新建 [路由分层设计](router-architecture.md),并在本节加入引用。

- **2026-10-27**(修正):`web/package-lock.json` 是前端 lockfile,应**保留**在版本控制中;此前误列入“移出”清单,已更正。同时补充 `server/log/.gitkeep` 以保留日志目录。
