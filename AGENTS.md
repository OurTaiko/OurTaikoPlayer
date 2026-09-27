# OurTaikoPlayer 维护约定

## 上游合并与回放适配（2026-09-27）

- 用户明确要求：当前游戏只记录输入，并随成绩上传回放数据；暂时不需要回放播放、请求、下载或轮询。不要接回上游 Hiroba 网络层，也不要添加游戏内回放入口。
- 成绩附带本局 `audio_offset`、`visual_offset`（毫秒）。输入按消费顺序保存，同一帧的多次敲击不能因时间戳相同而丢失。
- Fanmade 成绩表增加可空回放列；历史成绩、请求缺少回放字段或回放解析失败时保存 SQL NULL，正常成绩仍应受理。整个成绩请求损坏或成绩本身无效仍按原协议拒绝。
- 多服务器兼容：服务器未声明支持录制数据时，客户端继续发送原成绩格式。持久队列重试必须保持原请求体和幂等键。
- 用户要求分步 Conventional Commits：上游合并及冲突解决单独一个 merge commit；后续回放上传等功能改造另作 feature commit，不混在合并提交中。
- 本地测试通过，特别是确认缺少回放字段不会使服务器拒收之后，才允许连接 `ssh ourtaiko-prod`，再用 Docker 部署 Fanmade 后端。先检查实际部署布局，不能默认沿用旧 systemd 更新脚本。
- 本次同时涉及相邻 `OurTaiko/Fanmade/backend`；其部署和数据库约定以该目录的 AGENTS.md 为准。不得将 Fanmade 部署到 ESE 服务器或修改 ESE 数据。

## 验证

- 合并检查：`python3 tests/upstream/run.py`。游戏联网回归使用 `tests/fanmade/client.cpp` 和 `tests/fanmade/fixture.py`。
- 数据库/HTTP 验证需要独立本地测试 PostgreSQL schema，未设置 DATABASE_TEST_URL 而跳过的测试不算通过。
- 区分自动化验证、模拟器构建与真机游玩；不以编译成功声称所有平台均已验证。
