# 自动宣传奖励 Web 门户 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有 Flask `AuthServer` 中增加玩家宣传提交和管理员审核页面，直接连接 AzerothCore 的 auth/world/characters 三库，但把实际发奖和追回留给 worldserver 核心计划。

**Architecture:** Flask 只负责游戏账号 SRP6 登录、次数/内容预检、文件存储、提交和审核状态写入。玩家提交创建 `_宣传提交记录` 与 `_宣传奖励流水`，worldserver 消费后发 CDK或绑定物品；管理员审核只更新状态和回滚请求，不直接修改背包、物品或封号。

**Tech Stack:** Python 3、Flask、PyMySQL、Pillow、Requests、Jinja2、pytest、AzerothCore SRP6 `salt/verifier`。

---

## 文件边界

- `D:/AZ艾萨拉网关/源码/AuthServer/promotion_config.py`: 三库连接、上传和 URL 参数配置。
- `D:/AZ艾萨拉网关/源码/AuthServer/promotion_db.py`: auth/world/characters 连接和事务辅助。
- `D:/AZ艾萨拉网关/源码/AuthServer/promotion_auth.py`: SRP6 密码校验、账号和角色读取。
- `D:/AZ艾萨拉网关/源码/AuthServer/promotion_security.py`: CSRF、图片、URL、IP安全检查。
- `D:/AZ艾萨拉网关/源码/AuthServer/promotion_service.py`: 配额、提交、审核和数据库状态转换。
- `D:/AZ艾萨拉网关/源码/AuthServer/promotion_routes.py`: 玩家和管理员 HTTP 路由。
- `D:/AZ艾萨拉网关/源码/AuthServer/templates/promotion_player.html`: 玩家上传/链接提交页面。
- `D:/AZ艾萨拉网关/源码/AuthServer/templates/promotion_review.html`: 管理员审核页面。
- `D:/AZ艾萨拉网关/源码/AuthServer/static/promotion.css`: 页面样式。
- `D:/AZ艾萨拉网关/源码/AuthServer/promotion_config.example.json`: 不含密码的配置样例。
- `D:/AZ艾萨拉网关/源码/AuthServer/tests/`: SRP6、预检、配额、路由和状态测试。
- Modify: `D:/AZ艾萨拉网关/源码/AuthServer/app.py`、`requirements.txt`、`run.bat`。

## Task 1: 建立 Web 三库配置和连接层

**Files:**
- Create: `D:/AZ艾萨拉网关/源码/AuthServer/promotion_config.py`
- Create: `D:/AZ艾萨拉网关/源码/AuthServer/promotion_db.py`
- Create: `D:/AZ艾萨拉网关/源码/AuthServer/promotion_config.example.json`
- Create: `D:/AZ艾萨拉网关/源码/AuthServer/tests/test_promotion_config.py`
- Modify: `D:/AZ艾萨拉网关/源码/AuthServer/requirements.txt`

- [ ] **Step 1: Write the failing configuration tests**

```python
def test_load_promotion_config_requires_all_azerothcore_databases(monkeypatch, tmp_path):
    config_path = tmp_path / "promotion.json"
    config_path.write_text(
        '{"auth": {}, "world": {}, "characters": {}, "upload_root": "uploads"}',
        encoding="utf-8",
    )
    monkeypatch.setenv("PROMOTION_CONFIG_PATH", str(config_path))
    with pytest.raises(ValueError, match="auth.*world.*characters"):
        load_promotion_config()


def test_database_names_are_not_taken_from_request(monkeypatch):
    cfg = load_test_config()
    assert cfg.auth.database == "acore_auth"
    assert cfg.world.database == "acore_world"
    assert cfg.characters.database == "acore_characters"
```

- [ ] **Step 2: Run the tests and verify they fail**

Run from `D:\AZ艾萨拉网关\源码`:

```powershell
python -m pytest AuthServer/tests/test_promotion_config.py -q
```

Expected: FAIL because the promotion modules do not exist.

- [ ] **Step 3: Implement explicit configuration loading**

Load only `PROMOTION_CONFIG_PATH`; refuse to start the promotion blueprint when the file is absent or any of the three database blocks lacks `host`, `port`, `user`, `password`, or the fixed database name. The example file uses deployment-time environment expansion and never stores real credentials:

```json
{
  "auth": {"host": "127.0.0.1", "port": 3306, "database": "acore_auth", "user": "${AC_AUTH_DB_USER}", "password": "${AC_AUTH_DB_PASSWORD}"},
  "world": {"host": "127.0.0.1", "port": 3306, "database": "acore_world", "user": "${AC_WORLD_DB_USER}", "password": "${AC_WORLD_DB_PASSWORD}"},
  "characters": {"host": "127.0.0.1", "port": 3306, "database": "acore_characters", "user": "${AC_CHAR_DB_USER}", "password": "${AC_CHAR_DB_PASSWORD}"},
  "upload_root": "D:/AZ艾萨拉网关/源码/runtime/promotion_uploads",
  "max_upload_bytes": 8388608,
  "url_timeout_seconds": 5,
  "url_max_redirects": 3
}
```

`promotion_db.py` exposes `connect_auth()`, `connect_world()`, `connect_characters()` and `transaction(connection)`. Database names come from the loaded configuration, never from HTTP parameters; all SQL uses PyMySQL placeholders.

Before validation, expand `${NAME}` values from `os.environ`; fail closed when a referenced environment variable is missing.

- [ ] **Step 4: Add dependencies and rerun tests**

Add exact packages to `requirements.txt`: `Pillow>=10.0`, `requests>=2.31`, `pytest>=8.0`. Run:

```powershell
python -m pip install -r AuthServer/requirements.txt
python -m pytest AuthServer/tests/test_promotion_config.py -q
```

Expected: both configuration tests pass.

- [ ] **Step 5: Commit the connection layer**

```powershell
git -C 'D:\AZ艾萨拉网关\源码' add AuthServer/promotion_config.py AuthServer/promotion_db.py AuthServer/promotion_config.example.json AuthServer/tests/test_promotion_config.py AuthServer/requirements.txt
git -C 'D:\AZ艾萨拉网关\源码' commit -m "feat: add promotion web database configuration"
```

## Task 2: Implement game-account SRP6 login

**Files:**
- Create: `D:/AZ艾萨拉网关/源码/AuthServer/promotion_auth.py`
- Create: `D:/AZ艾萨拉网关/源码/AuthServer/tests/test_promotion_auth.py`

- [ ] **Step 1: Add an SRP6 test vector**

Use a fixed username, password, 32-byte little-endian salt and verifier generated by the existing `htdocs/api.php` implementation. Test that the correct password passes, a wrong password fails, and the original password is never written to a log or session value.

- [ ] **Step 2: Run the test and verify it fails**

```powershell
python -m pytest AuthServer/tests/test_promotion_auth.py -q
```

Expected: FAIL because `verify_srp6_password` is not defined.

- [ ] **Step 3: Port the verified SRP6 calculation**

Implement:

```python
N = int("894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7", 16)
G = 7


def calculate_verifier(username: str, password: str, salt: bytes) -> bytes:
    upper = f"{username.upper()}:{password.upper()}".encode("utf-8")
    h1 = hashlib.sha1(upper).digest()
    x = int.from_bytes(hashlib.sha1(salt + h1).digest(), "little")
    return pow(G, x, N).to_bytes(32, "little")


def verify_srp6_password(username: str, password: str, salt: bytes, verifier: bytes) -> bool:
    if len(salt) != 32 or len(verifier) != 32:
        return False
    return hmac.compare_digest(calculate_verifier(username, password, salt), verifier)
```

`authenticate_game_account()` queries `auth.account` for `id,username,salt,verifier,locked`, rejects locked accounts, and returns only account ID and normalized username. It must never include the submitted password in exceptions, logs, or Flask session data.

- [ ] **Step 4: Add character selection lookup**

Implement `list_characters(account_id)` against `characters` with `guid,name,level,race,class`, ordered by name. The player submission form must select one of these GUIDs; a GUID from another account returns HTTP 403.

- [ ] **Step 5: Run tests and commit**

```powershell
python -m pytest AuthServer/tests/test_promotion_auth.py -q
git -C 'D:\AZ艾萨拉网关\源码' add AuthServer/promotion_auth.py AuthServer/tests/test_promotion_auth.py
git -C 'D:\AZ艾萨拉网关\源码' commit -m "feat: authenticate promotion users with srp6"
```

Expected: all SRP6 and account/character ownership tests pass.

## Task 3: Implement safe content and URL prechecks

**Files:**
- Create: `D:/AZ艾萨拉网关/源码/AuthServer/promotion_security.py`
- Create: `D:/AZ艾萨拉网关/源码/AuthServer/tests/test_promotion_security.py`

- [ ] **Step 1: Write failing security tests**

Cover these exact cases:

```python
assert is_public_http_url("https://example.com/post")
assert not is_public_http_url("file:///C:/secret.txt")
assert not is_public_http_url("http://127.0.0.1/admin")
assert not is_public_http_url("http://169.254.169.254/latest/meta-data")
assert keyword_match("艾萨拉服务器宣传", ["艾萨拉", "宣传"])
assert not keyword_match("普通游戏帖子", ["艾萨拉", "宣传"])
```

- [ ] **Step 2: Run the tests and verify they fail**

```powershell
python -m pytest AuthServer/tests/test_promotion_security.py -q
```

Expected: FAIL because the URL and image helpers do not exist.

- [ ] **Step 3: Implement URL validation and keyword extraction**

Allow only `http` and `https`; resolve every hostname and reject loopback, private, link-local, multicast, unspecified, reserved, and documentation IP ranges. Fetch one response at a time with a five-second timeout and at most three redirects, rechecking the destination IP on every hop. Parse server-rendered HTML with `html.parser.HTMLParser`, normalize whitespace/case, and match configured keywords against title plus visible text. A page requiring JavaScript or login returns `PENDING_MANUAL`, not automatic rejection.

- [ ] **Step 4: Implement image validation and storage**

Read at most `max_upload_bytes`; verify the decoded format with Pillow, accept PNG/JPEG/WebP, strip metadata, re-encode to PNG, generate a UUID filename, and store outside Flask `static`. Return `sha256`, storage key, MIME type, width, height, and byte size. Reject executable content, oversized files, corrupt images, and duplicate hashes for the same account/task/day.

- [ ] **Step 5: Add CSRF and trusted-IP helpers**

Create a session token with `secrets.token_urlsafe(32)`. Accept `X-Forwarded-For` only when `REMOTE_ADDR` is in a configured trusted proxy list; otherwise use `request.remote_addr`. Use the resulting address for the IP quota, never a client-supplied form field.

- [ ] **Step 6: Run tests and commit**

```powershell
python -m pytest AuthServer/tests/test_promotion_security.py -q
git -C 'D:\AZ艾萨拉网关\源码' add AuthServer/promotion_security.py AuthServer/tests/test_promotion_security.py
git -C 'D:\AZ艾萨拉网关\源码' commit -m "feat: add promotion content security checks"
```

Expected: URL, SSRF, keyword, image, duplicate-hash, and CSRF tests pass.

## Task 4: Create player submission and quota service

**Files:**
- Create: `D:/AZ艾萨拉网关/源码/AuthServer/promotion_service.py`
- Create: `D:/AZ艾萨拉网关/源码/AuthServer/tests/test_promotion_service.py`

- [ ] **Step 1: Write quota and idempotency tests**

Test account, IP, and task limits independently; assert that an exact repeated `request_id` returns the original submission and that a different request is rejected once any configured limit is reached.

- [ ] **Step 2: Implement transactionally locked quota reservation**

Within one characters-database transaction:

1. `SELECT` the world task configuration and verify `启用=1`.
2. `INSERT ... ON DUPLICATE KEY UPDATE` the current row in `_宣传账号统计` and `_宣传IP统计`.
3. `SELECT ... FOR UPDATE` both rows.
4. Reject when account, IP, or task count reaches its limit.
5. Increment both counters and insert `_宣传提交记录`.
6. Insert `_宣传奖励流水` with a UUID `request_id`, `模式`, and `发放状态='REQUESTED'`.
7. Commit only after all seven operations succeed.

The service receives a normalized account ID, selected character GUID, IP, content reference, content hash, task ID, and submission type. It verifies the character belongs to the account before reserving quota.

- [ ] **Step 3: Implement review persistence**

`approve_submission()` and `reject_submission()` must use a compare-and-set update from `审核状态='PENDING'`, insert `_宣传审核日志`, and return the updated row. Rejection sets grant `回滚状态='REQUESTED'`; worldserver performs the actual rollback and ban. A second decision returns the already-final result without changing counters.

- [ ] **Step 4: Run service tests**

```powershell
python -m pytest AuthServer/tests/test_promotion_service.py -q
```

Expected: all transaction, quota, ownership, and idempotency tests pass against the fake repository.

- [ ] **Step 5: Commit the service**

```powershell
git -C 'D:\AZ艾萨拉网关\源码' add AuthServer/promotion_service.py AuthServer/tests/test_promotion_service.py
git -C 'D:\AZ艾萨拉网关\源码' commit -m "feat: add promotion submission service"
```

## Task 5: Add player and admin routes

**Files:**
- Create: `D:/AZ艾萨拉网关/源码/AuthServer/promotion_routes.py`
- Create: `D:/AZ艾萨拉网关/源码/AuthServer/templates/promotion_player.html`
- Create: `D:/AZ艾萨拉网关/源码/AuthServer/templates/promotion_review.html`
- Create: `D:/AZ艾萨拉网关/源码/AuthServer/static/promotion.css`
- Create: `D:/AZ艾萨拉网关/源码/AuthServer/tests/test_promotion_routes.py`
- Modify: `D:/AZ艾萨拉网关/源码/AuthServer/app.py`

- [ ] **Step 1: Write route tests**

Use Flask’s test client to assert:

```python
assert client.post("/promotion/login", data={"username": "TEST", "password": "bad"}).status_code == 401
assert client.get("/promotion").status_code == 302
assert client.post("/admin/promotions/1/reject", data={"reason": "关键词不匹配"}).status_code == 302
```

Also assert that an unauthenticated request cannot access either the player submit endpoint or admin review endpoint.

- [ ] **Step 2: Register a blueprint without changing existing license routes**

`promotion_routes.py` defines a `Blueprint("promotion", __name__)`; `app.py` imports it after existing helper definitions and calls `app.register_blueprint(promotion_bp)`. Existing `/api/verify`, `/admin`, and license pages keep their current route names.

- [ ] **Step 3: Implement player login and submission routes**

Implement:

```text
GET  /promotion/login
POST /promotion/login
GET  /promotion
POST /promotion/submit
GET  /promotion/submissions/<int:submission_id>
POST /promotion/logout
```

Successful login stores only `promotion_account_id`, `promotion_username`, `promotion_csrf`, and the selected character GUID in the session. `POST /promotion/submit` accepts either one image or one URL, runs the security precheck, calls `create_submission()`, and returns `PENDING_REVIEW` with the submission ID. It never generates a CDK or edits character state.

- [ ] **Step 4: Implement admin review routes**

Implement:

```text
GET  /admin/promotions
GET  /admin/promotions/<int:submission_id>
POST /admin/promotions/<int:submission_id>/approve
POST /admin/promotions/<int:submission_id>/reject
POST /admin/promotions/<int:submission_id>/retry
```

Reuse the existing `login_required` decorator and require `session['admin_role'] >= 0`; require a CSRF token and a non-empty rejection reason. `approve`/`reject` call the service methods and only update characters-side review tables. `retry` only resets `回滚状态='REQUESTED'` for a `RECOVERY_DEBT` row; it never deletes items from Web code.

- [ ] **Step 5: Render safe review content**

The player page shows quota counters, selected character, upload preview, URL result, reward mode, and submission status. The review page shows image through a controlled download route, sanitized URL/title/text, automatic check result, CDK/role/item receipt, rollback state, and the full audit history. Jinja autoescaping remains enabled; no raw HTML from external pages is rendered.

- [ ] **Step 6: Run route tests and commit**

```powershell
python -m pytest AuthServer/tests/test_promotion_routes.py -q
git -C 'D:\AZ艾萨拉网关\源码' add AuthServer/promotion_routes.py AuthServer/templates/promotion_player.html AuthServer/templates/promotion_review.html AuthServer/static/promotion.css AuthServer/tests/test_promotion_routes.py AuthServer/app.py
git -C 'D:\AZ艾萨拉网关\源码' commit -m "feat: add promotion player and review routes"
```

Expected: login, session, CSRF, player submission, admin review, and recovery retry tests pass.

## Task 6: Configure deployment and perform end-to-end validation

**Files:**
- Modify: `D:/AZ艾萨拉网关/源码/AuthServer/run.bat`
- Modify: `D:/AZ艾萨拉网关/源码/AuthServer/requirements.txt`
- Create: `D:/AZ艾萨拉网关/源码/AuthServer/tests/test_promotion_live_contract.py`

- [ ] **Step 1: Add a non-secret startup check**

`run.bat` sets `PROMOTION_CONFIG_PATH` only when the variable is not already defined, starts Flask with debug disabled, and exits with a clear message if the config file is missing. It must not echo database passwords.

- [ ] **Step 2: Run static and route checks**

From `D:\AZ艾萨拉网关\源码`:

```powershell
python -m pytest AuthServer/tests -q
python -m flask --app AuthServer.app routes | Select-String promotion
```

Expected: all tests pass and the output lists every `/promotion` and `/admin/promotions` route.

- [ ] **Step 3: Run controlled database integration**

With a test account and a staging copy of the three AzerothCore databases:

1. Login with the test account and select its character.
2. Submit one keyword-matching public URL and one valid image.
3. Query `_宣传提交记录` and `_宣传奖励流水`; both rows must be `PENDING`/`REQUESTED` and no game reward exists before worldserver consumes them.
4. Let worldserver process the queue; verify exactly one CDK or bound item is created.
5. Approve one submission and reject the other from the admin page; verify the core worker finalizes/rolls back and writes `_宣传审核日志`.
6. Repeat rejection until the configured threshold and verify `auth.account_banned` plus account disconnect.

- [ ] **Step 4: Commit deployment checks**

```powershell
git -C 'D:\AZ艾萨拉网关\源码' add AuthServer/run.bat AuthServer/requirements.txt AuthServer/tests/test_promotion_live_contract.py
git -C 'D:\AZ艾萨拉网关\源码' commit -m "test: validate promotion web deployment"
```
