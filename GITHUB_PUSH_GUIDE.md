# GitHub 发布指南

本指南帮助你将适配后的 pg_tde 推送（push）到自己的 GitHub 仓库。

---

## 方式一：通过 GitHub Personal Access Token（推荐）

### 第一步：创建 GitHub 账号（如尚未拥有）

1. 访问 https://github.com
2. 点击 **Sign up** 创建账号（需要邮箱验证）
3. 完成后继续第二步

### 第二步：创建 Personal Access Token

1. 登录 GitHub → 点击右上角头像 → **Settings**
2. 左侧菜单 → **Developer settings**
3. **Personal access tokens** → **Tokens (classic)**
4. 点击 **Generate new token (classic)**
5. 配置：
   - **Name**: `pg_tde-push`（任意名称）
   - **Expiration**: 建议 30 天
   - **Scopes**: 勾选 `repo`（完整仓库访问）
6. 点击 **Generate token**
7. **⚠️ 立即复制 Token**，关闭页面后无法再查看

### 第三步：本地配置 git remote 并推送

在有 `git` 和 `curl` 环境的机器上执行：

```bash
cd /workspace/pg_tde

# 添加远程仓库（创建新仓库）
git remote add origin https://github.com/YOUR_USERNAME/pg_tde_open_src_pg.git

# 或如果已经添加了 origin：
git remote set-url origin https://github.com/YOUR_USERNAME/pg_tde_open_src_pg.git

# 推送到 GitHub（会提示输入用户名和密码/Token）
git push -u origin main

# 用户名：你的 GitHub 用户名
# 密码：粘贴你的 Personal Access Token（不是 GitHub 密码！）
```

**或者用 GitHub CLI 更简单：**

```bash
# 安装 GitHub CLI（如果尚未安装）
# macOS: brew install gh
# Linux: sudo apt install gh 或 sudo yum install gh

gh auth login
# 选择 GitHub.com → HTTPS → 登录

cd /workspace/pg_tde
gh repo create pg_tde_open_src_pg --public --source=. --push
```

---

## 方式二：直接通过浏览器上传（无需配置 git）

如果不想配置 git，可以在 GitHub 网页上手动上传：

1. 登录 GitHub → 点击右上角 **+** → **New repository**
2. 填写：
   - **Repository name**: `pg_tde_open_src_pg`
   - **Description**: `pg_tde 适配开源 PostgreSQL - 表数据加密（无 Percona 依赖）`
   - **Public** 或 **Private**
   - **不要**勾选 "Initialize this repository with a README"
3. 点击 **Create repository**
4. 在仓库页面，点击 **uploading an existing file**
5. 将 `/workspace/pg_tde` 目录下的所有文件拖拽上传
6. 点击 **Commit changes**

⚠️ 注意：此方式无法保留 git 历史记录，建议使用方式一。

---

## 验证仓库

推送成功后，访问：

```
https://github.com/YOUR_USERNAME/pg_tde_open_src_pg
```

应该能看到：
- `README.md`
- `Makefile.open_src_pg`
- `scripts/build.sh`
- 完整的 `src/` 源码目录

---

## 克隆到其他机器

```bash
git clone https://github.com/YOUR_USERNAME/pg_tde_open_src_pg.git
cd pg_tde_open_src_pg
./scripts/build.sh
```

---

## 常见问题

### Q: 推送时提示 "Authentication failed"

**原因**：密码输入错误（应该用 Personal Access Token，不是 GitHub 密码）

**解决**：
```bash
# 清除保存的凭证
git credential-cache exit  # macOS: git credential-osxkeychain erase

# 重新推送，输入正确的 Token
git push -u origin main
```

### Q: 推送时提示 "remote repository not found"

**原因**：仓库名或用户名错误

**解决**：检查 `git remote -v` 确认 URL 正确

### Q: 提示 "refusing to push to protected branch"

**原因**：主分支被保护

**解决**：
```bash
git push -u origin main -f
```

---

## 后续步骤

推送成功后，建议：

1. **创建 Releases**：为项目发布一个版本（如 v2.1.2-open-src）
2. **添加 Topics**：在仓库设置中添加 `postgresql`, `encryption`, `database`, `security`
3. **写一个中文 README**：补充中文使用说明
4. **分享**：将仓库链接分享给需要的人
