# Gitea 成员使用指南

这是团队内部的 Git 服务。开始前，请向管理员领取 **Gitea 网页地址、个人账号和仓库链接**。协作时优先从仓库页面复制克隆地址，里面已包含正确的主机名和端口。

## 第一次使用

1. 在管理员指定的网络上打开 Gitea，登录自己的账号。如果收到临时密码，首次登录后改成自己的密码。
2. 到仓库页面确认能看到代码。只需要看代码和克隆，请申请读取权限。需要推送自己的分支，请申请写入权限。
3. 如果用 SSH，在电脑上找到已有的公钥，或运行 `ssh-keygen -t ed25519` 来生成一对。到 **个人设置 → SSH / GPG 密钥** 添加 `.pub` 文件里的公钥，私钥留在电脑上。第一次 SSH 连接时，和管理员核对 Gitea 的主机密钥指纹。
4. 如果使用 HTTPS 克隆，按管理员提供的认证方式登录。需要令牌时，把它交给 Git 凭据管理器，克隆地址保持为普通仓库 URL。

[仓库与团队权限](https://docs.gitea.com/zh-cn/usage/access-control/permissions/) · [Gitea 常见问题](https://docs.gitea.com/zh-cn/help/faq/)

## 提交代码

从已有仓库开始。把下例 `ssh://…` 换成你在网页上复制的地址，并按实际仓库名进入目录。

```bash
git clone ssh://git@git.example.org:2221/team/demo.git
cd demo
git switch -c feat/my-change

# 修改文件后，按实际路径暂存
git add path/to/file
git commit -m "说明这次改动"
git push -u origin feat/my-change
```

推送后会有创建 Pull Request 的入口。选择你的分支作为来源，目标通常是 `main`，写清改动、测试情况和需要评审的地方。再往同一个分支推送提交，原 PR 会更新。如果项目启用了 Git LFS，按仓库说明安装 Git LFS 后再拉取大文件。[Gitea 推送说明](https://docs.gitea.com/zh-cn/usage/repository/push/) · [Git LFS 设置](https://docs.gitea.com/administration/git-lfs-setup/)

## 权限和发版

| 你想做的事 | 怎么做 |
| --- | --- |
| 查看、克隆私有库 | 让管理员把你加入对应仓库或团队 |
| 提交改动 | 推送自己的分支，再开 PR |
| 更新受保护的 `main` | 走仓库规定的 PR 和评审流程 |
| 发布 `v1.2.3` 这类标签 | 找有发版权限的人推送标签并创建 Release |

[保护分支](https://docs.gitea.com/usage/access-control/protected-branches/) · [受保护的标签](https://docs.gitea.com/zh-cn/usage/access-control/protected-tags/)

## 卡住时先看这里

- **打不开网页**。核对管理员给的地址和端口，并确认电脑在能访问 Gitea 的网络上。
- **登录了但看不到仓库**。把仓库链接和你的用户名发给管理员核对授权。
- **SSH 显示 `Permission denied (publickey)`**。核对你复制的是仓库的 SSH 地址、端口，以及对应公钥是否已加到正确的 Gitea 账号。
- **能克隆但推送被拒绝**。先看当前分支和目标分支，再查看报错是写权限不足还是分支、标签受保护。

需要脚本调用 API 时，在 **个人设置 → 应用** 创建用途单一的访问令牌。例如，只查询仓库用 `read:repository`。把令牌交给脚本的凭据存储，使用结束后撤销。需要第三方应用用 Gitea 登录时，把应用的回调地址和用途给管理员确认。[API 使用指南](https://docs.gitea.com/development/api-usage/) · [OAuth2 提供者](https://docs.gitea.com/zh-cn/development/oauth2-provider/)
