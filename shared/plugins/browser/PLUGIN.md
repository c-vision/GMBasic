---
name: browser
description: Headless Chromium automation via CDP (navigate, extract text, run JS, screenshots).
trigger: browser, navigate, open page, screenshot, extract text from page, web automation
version: 1.0.0
enabled: true
tools: [browser_navigate, browser_text, browser_title, browser_evaluate, browser_screenshot, browser_set_headers, browser_set_fetch_headers, browser_set_cookie, browser_set_user_agent, browser_auth, browser_get_cookies, browser_connect, browser_set_timeout, browser_wait_for, browser_click, browser_fill, browser_scroll, browser_cdp_error, browser_download, browser_analyze, browser_download_pdf, browser_set_cookie_persistent, browser_to_rag, browser_ping, browser_assert, browser_human_click, browser_human_type, browser_human_scroll, browser_profile, browser_block_urls, browser_robots_allowed, browser_rate_limit, browser_check_captcha, browser_replay, browser_task_save, browser_task_resume, browser_audit_step, browser_audit_export]
requires: [NET, HTTP, STRING, CRYPTOGRAPHY]
procedures: [BrowserNavigate, BrowserText, BrowserTitle, BrowserEvaluate, BrowserScreenshot, BrowserWait, BrowserSetHeaders, BrowserSetFetchHeaders, BrowserSetCookie, BrowserSetUserAgent, BrowserAuth, BrowserGetCookies, BrowserConnect, BrowserSetTimeout, BrowserWaitFor, BrowserClick, BrowserFill, BrowserScroll, BrowserCdpError, BrowserDownload, BrowserAnalyze, BrowserDownloadPdf, BrowserSetCookiePersistent, BrowserToRag, BrowserPing, BrowserAssert, HumanClick, HumanType, HumanScroll, BrowserProfile, BrowserBlockUrls, BrowserRobotsAllowed, BrowserRateLimit, BrowserCheckCaptcha, BrowserReplay, BrowserTaskSave, BrowserTaskResume, BrowserAuditStep, BrowserAuditExport]
---
# Plugin browser

Operating instructions for the agent: use `AI.PLUGIN.CALL("browser", ...)` to drive a headless
Chromium browser. The `BrowserNavigate(url, port)` procedure opens a page; the reading/interaction
capabilities use the Chrome DevTools Protocol (CDP) over WebSocket.
Requires Chromium started with `--remote-debugging-port` (see `launch_chromium.sh`).
