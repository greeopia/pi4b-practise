# 树莓派 Gitea 管理员指南

适用方案是 **Pi 4B（2 GB，Debian arm64）+ Gitea + SQLite**。假设先在内网试用，再邀请其他人。按之前的端口规划，Gitea 网页占 `8080`，Git over SSH 占 `2221`，系统 SSH 负责进 Pi 管理。团队最终使用的地址确定后，再填到成员指南里。

## 部署前先处理 Docker

本次只读检查发现 Docker 守护进程启动失败，`sudo docker compose version` 也无法识别 Compose。包管理器有两个相关包的安装记录，磁盘上缺失它们登记的文件。

| 软件包 | 实际缺失的文件 | 影响 |
| --- | --- | --- |
| `docker-compose-plugin` | `/usr/libexec/docker/cli-plugins/docker-compose` | CLI 找不到 `docker compose` |
| `iptables` | `/usr/lib/aarch64-linux-gnu/xtables/libxt_addrtype.so` | Docker 创建桥接网络时无法加载 `addrtype` |

`modprobe --dry-run --verbose xt_addrtype` 找到了当前内核的模块文件。`apt-get -s --reinstall --no-remove install docker-compose-plugin iptables` 的模拟结果只有两项重装。部署前先补齐两个包并启动 Docker，再验证 `systemctl is-active docker`、`sudo docker compose version` 和 `sudo docker info`。[Docker 的 Debian 安装说明](https://docs.docker.com/engine/install/debian/)

## 首版起哪些服务

| 服务 | 什么时候加 | 原因 |
| --- | --- | --- |
| **Gitea + SQLite** | 首版 | Git、PR、工单、镜像与 Webhook 先用 Gitea 自带的功能。SQLite 在同一容器里，持久化卷只有一处 |
| **sqlite-web** | 排查数据库时 | 维持之前的 `tools` profile、只读挂载和 Pi 回环端口。先完成 Gitea 安装，再单独启动查看 |
| **Caddy** | 准备给其他人发地址时 | HTTPS 保护网页登录和 Git/API 令牌传输，给团队一个固定入口 |
| **Gitea Runner** | 有第一个明确的 CI 任务时 | 把 STM32、ESP32 或 NuttX 编译放在专用构建机或隔离 VM 上。Runner 会执行仓库工作流，接入宿主 Docker 的任务会共享该守护进程 |
| **PostgreSQL** | 已计划持续的并发写入或明显扩容时 | 这是有负载时再做的数据库选择。如果提前确定会增长，可以在首次安装时直接选它，Gitea 官方提醒跨数据库迁移的测试有限 |

[Gitea Docker 安装](https://docs.gitea.com/zh-cn/installation/install-with-docker/) · [数据库选择](https://docs.gitea.com/installation/database-prep/) · [Gitea Runner](https://docs.gitea.com/runner/installation/docker/)

## 配置和上线顺序

保留上一份建议的 `gitea:/data`、SQLite 和 `2221:22`。以下是可以合并到 Gitea 服务段中的设置。

```yaml
services:
  gitea:
    environment:
      GITEA__service__DISABLE_REGISTRATION: "true"
      GITEA__service__REQUIRE_SIGNIN_VIEW: "true"
      GITEA__repository__DEFAULT_PRIVATE: private
      GITEA__repository__ENABLE_PUSH_CREATE_USER: "false"
      GITEA__repository__ENABLE_PUSH_CREATE_ORG: "false"
      GITEA__actions__ENABLED: "false"
    logging:
      driver: json-file
      options:
        max-size: "10m"
        max-file: "3"
```

- **关闭公开注册，要求访问者先登录，新仓库默认私有。** 首次安装页创建站点管理员。这个仓库设置是新建时的默认值，建库后核对可见性。
- **用固定入口发链接。** 初次在 Pi 上试用时，把基础 URL 填成实际打开的 `http://树莓派地址:8080/`。组队前选一个可解析到 Pi 的固定名字。使用 Caddy 时，让它在同一 Compose 网络转发到 `gitea:3000`，网页入口发布 Caddy 的 `443`，Gitea 的 `ROOT_URL` 改成真实的 HTTPS 地址。用内网 CA 时持久化 Caddy 的 `/data`，给成员分发 CA 的公开根证书并确认浏览器和 Git 信任它。
- **固定 Compose 项目名。** 第一次启动前可在顶层加 `name: gitea`，避免之后移动目录改变卷名。已有实例时继续使用原项目名。
- **控制资源积累。** 容器日志单独轮转。保留 Git LFS 时也把 LFS 对象算进磁盘和备份。备份完成后再升级固定版本的镜像。
- **SMTP 有需要再启用。** 如果需要自助找回密码或通知，再填 `SMTP_ADDR`、`SMTP_PORT`、发件地址与凭据，并用管理页发测试邮件。离线试用时由管理员处理账号。
- **先用普通成员账号验收。** 检查登录、私有仓库可见性、SSH 克隆、推送功能分支和打开 PR，再把成员指南发出去。

[配置速查表](https://docs.gitea.com/administration/config-cheat-sheet/) · [Docker 日志轮转](https://docs.docker.com/engine/logging/drivers/json-file/) · [Gitea 反向代理](https://docs.gitea.com/administration/reverse-proxies/) · [Caddy 内网 HTTPS](https://caddyserver.com/docs/running#local-https-with-docker) · [SMTP 设置](https://docs.gitea.com/administration/email-setup/)

## 成员、分支和发版

1. 给每个人创建独立账号。一个仓库可以直接添加协作者。共享多个仓库时用组织和团队，按仓库和功能单元给权限。
2. 只需要拉代码的人给 Code Read，贡献者给 Code Write，负责维护仓库设置的人再给仓库 Admin。用站点管理员账号处理用户和全站配置。
3. 建好 `main` 后，在 **仓库设置 → 分支** 保护它，关闭直接推送和强推。需要审核时设一个有效的审批，并确认至少有一位可审核的成员。
4. 在 **仓库设置 → 标签** 用 `v*` 保护发版标签，并选中允许发版的成员或团队。规则的允许名单为空时，创建和修改都被拦截。用普通成员和发版成员分别验证一次。

[仓库权限](https://docs.gitea.com/zh-cn/usage/access-control/permissions/) · [保护分支](https://docs.gitea.com/usage/access-control/protected-branches/) · [保护标签](https://docs.gitea.com/zh-cn/usage/access-control/protected-tags/) · [推送功能](https://docs.gitea.com/zh-cn/usage/repository/push/)

## 备份、恢复和升级

首版用停机备份。等写入任务结束后停 Gitea，再备份整个 `/data` 卷，这样 SQLite、仓库、附件、LFS 和 `app.ini` 对应到同一个时间点。把 Compose 和私下使用的环境配置一并保存在受控位置。数据库和配置里可能含账户数据及密钥。

以下命令供部署后在 **Pi 的 Compose 目录**手动执行。它复用正在运行的 Gitea 镜像里的 `tar`，用临时容器只读打包，退出时尝试恢复 Gitea 服务。

```bash
(
  set -eu
  umask 077
  container=$(sudo docker compose ps -q gitea)
  test -n "$container"
  volume=$(sudo docker inspect --format '{{range .Mounts}}{{if and (eq .Type "volume") (eq .Destination "/data")}}{{.Name}}{{end}}{{end}}' "$container")
  image=$(sudo docker inspect --format '{{.Image}}' "$container")
  test -n "$volume"

  mkdir -p backups
  partial=$(mktemp --suffix=.tar.gz.part "backups/gitea-$(date +%F)-XXXXXX")
  cleanup() {
    rc=$?
    trap - EXIT
    sudo docker compose start gitea >&2 || rc=1
    rm -f -- "$partial"
    exit "$rc"
  }
  trap cleanup EXIT

  sudo docker compose stop gitea
  sudo docker run --rm --pull=never --network none \
    --mount "type=volume,source=$volume,target=/data,readonly" \
    --entrypoint tar "$image" -czf - -C /data . > "$partial"
  test -s "$partial"
  tar -tzf "$partial" >/dev/null
  archive=${partial%.part}
  mv -- "$partial" "$archive"
  sha256sum "$archive"
)
```

由电脑主动通过有线 SSH 拉取一份备份到另一块盘。首次备份后，在隔离环境用**同版本 Gitea、独立卷和测试端口**演练恢复，验证登录、克隆和 LFS 文件。升级前再做一份新备份，阅读目标版本发布说明，换镜像后用网页和一次测试仓库的 clone、push 验收。[Gitea 备份与恢复](https://docs.gitea.com/administration/backup-and-restore/) · [Docker 数据卷备份](https://docs.docker.com/engine/storage/volumes/#back-up-restore-or-migrate-data-volumes)

## 镜像、Webhook 和 API

| 需求 | 用法 |
| --- | --- |
| 把一个上游仓库同步到 Pi | 新建迁移时勾选拉取镜像，先在仓库设置点一次立即同步。已有普通仓库需要另建拉取镜像 |
| 把 Pi 上的仓库同步到远端 | 在仓库设置添加推送镜像。先用明确的专用目标仓库和最小权限凭据测试，确认目标分支和标签可以被镜像同步覆盖 |
| 有 push 或 PR 时通知其他服务 | 配置仓库级 Webhook，只选接收方需要的事件。用测试投递核对 URL、返回码和投递记录。接收端使用原始请求体验证 `X-Gitea-Signature` 的 HMAC-SHA256。Gitea 28.4 的内网出站规则可以用 `GITEA__security__ALLOWED_HOST_LIST` 精确放行接收端 IP 和端口 |
| 自己写脚本查仓库 | 在个人设置的应用页建专用 PAT，为读取任务选 `read:repository`。用 HTTPS 和 `Authorization: token …`，接口路径从实例的 `/api/swagger` 查 |
| 给第三方工具接入用户登录 | 在应用页注册 OAuth2 客户端，填它提供的回调地址。用 Authorization Code 流程，校验 `state`，支持时启用 PKCE。需要 OIDC 时用实例的发现文档 |

镜像同步 Git 引用和提交。账号、权限、工单、PR 和附件由完整的 `/data` 备份覆盖。[仓库镜像](https://docs.gitea.com/zh-cn/usage/repository/repo-mirror/) · [Webhook](https://docs.gitea.com/zh-cn/usage/repository/webhooks/) · [28.4 出站配置](https://github.com/go-gitea/gitea/blob/v28.4.0/custom/conf/app.example.ini#L532) · [API 令牌权限](https://docs.gitea.com/development/api-usage/) · [OAuth2 / OIDC](https://docs.gitea.com/zh-cn/development/oauth2-provider/)

API 最小例子。用你的 HTTPS 地址和仓库名，先通过凭据存储给 shell 提供 `GITEA_TOKEN`。

```bash
GITEA_URL=https://git.example.org
curl -fsS -H "Authorization: token ${GITEA_TOKEN:?请提供 Gitea 访问令牌}" \
  "${GITEA_URL%/}/api/v1/repos/team/demo"
```

## 从 Awesome Gitea 选什么

- **`tea`** 放在你自己的电脑上。适合经常从终端查看 PR 和工单。
- **Renovate** 等有几个持续维护的仓库再考虑。让它开依赖升级 PR，由你审核。首版先手动固定和更新 Gitea 镜像。
- **镜像先用 Gitea 自带功能。** 如果有一批 GitHub 仓库要统一同步，再看第三方的 Gitea Mirror，并先检查相应账号令牌的权限。

[Awesome Gitea 文档版](https://docs.gitea.com/awesome/) · [Renovate 的 Gitea 支持](https://docs.renovatebot.com/modules/platform/gitea/) · [Gitea Mirror 项目](https://github.com/RayLabsHQ/gitea-mirror)

定位使用问题时，先区分是 **Pi 上 Docker 服务、网页入口、SSH 公钥，还是 Gitea 的仓库权限**。网页打不开先看 `sudo docker compose ps` 和 `sudo docker compose logs --tail=100 gitea`。网页能用且 SSH 报错时，从仓库页面重新核对端口和克隆地址。[Gitea 常见问题](https://docs.gitea.com/zh-cn/help/faq/)
