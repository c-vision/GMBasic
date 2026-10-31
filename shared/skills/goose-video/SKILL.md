---
name: goose-video
slug: goose-video
description: >
  GooseWorks video ads — remix a video ad template (iMessage chat-reveal, more coming) into a
  branded video ad for the user's product. Renders LOCALLY on the user's machine (Playwright +
  ffmpeg + GooseWorks media proxies) and saves the finished MP4 back to the project over MCP.
  Use when the user says "make the video for project <id>", references a video ad project or
  template, or asks to remix a video ad. Unlike goose-ads (static images, generated server-side),
  video renders locally and reports progress + the result back through the gooseworks MCP tools.
category: ads
version: 0.1.0
author: GooseWorks
tags: [gooseworks, ads, video, remix, imessage, local-render, byoa]
---

# GooseWorks Video Ads — local remix runtime

You produce **video** ad creative on the user's OWN machine and sync the result back to the
GooseWorks app over MCP. This document is the **runtime contract** (auth, credits, the media
proxies, data I/O, the review gate). A separate **recipe skill** — fetched per format — tells
you *what to make*; read both, and this doc wins on any conflict about the environment.

You run inside the user's own Claude Code session (they pasted an instruction with a project
id). The app NEVER runs you — it is the viewer + review surface; you are the renderer.

## Prerequisite — MCP + a local toolchain (Phase 0 preflight)

- The `mcp__gooseworks__*` tools are REQUIRED. If they're unavailable, stop and tell the user
  to run `gooseworks install --claude --mcp` and restart Claude Code. There is no REST fallback.
- This is a LOCAL render. Run `gooseworks doctor` FIRST — it checks login, the MCP server,
  **ffmpeg** + **ffprobe**, and **Playwright Chromium** in one shot. If it reports any ✗, relay
  the exact fix it prints (e.g. `brew install ffmpeg`, `npx playwright install chromium`) and
  stop — don't half-render.

## Identity, token, credits

- Read `~/.gooseworks/credentials.json` → `api_key` (your agent token), `api_base`, `agent_id`.
  Never print the token.
- **CRITICAL — target the org-default Ads agent on EVERY file op.** The app serves project files
  (the render-file route) from the org's DEFAULT agent, but MCP file writes default to your
  token's pinned agent — which can be a DIFFERENT agent, so a render written with the default
  scope is **invisible in the app**. First resolve the Ads agent: `list_accessible_scopes` → the
  scope with `is_org_default: true` (the ORG default — NOT the `is_default` / `default_agent_id`
  fields, which are the *user's* default agent and are often a DIFFERENT agent). Its `agent_id` is
  `ADS_AGENT` (name "Ads agent", slug `org-default`; usually also the `agent_id` in
  credentials.json). Then pass `target: { type: "agent", agent_id: ADS_AGENT }` on EVERY
  `get_upload_url` / `get_download_url` / `write_file` / `list_directory` / `read_file` — NEVER
  omit `target`.
- **CRITICAL — publish under the PROJECT FOLDER, not the workspace root (the #1 "video renders but
  is invisible" bug).** `get_upload_url` stores at `<ADS_AGENT>/files/<path>` verbatim, but the
  render-file route reads from
  `<ADS_AGENT>/files/agent-config/brands/<brand_slug>/projects/<project_id>/<path>`
  (see backend `resolveProjectFileKey`). So EVERY publish/preview `path` MUST be prefixed with
  `agent-config/brands/<brand_slug>/projects/<project_id>/` — e.g. upload to
  `agent-config/brands/<brand_slug>/projects/<project_id>/working/final.mp4`, NEVER bare
  `working/final.mp4`. A bare path 404s in the app even though the render row AND a bare-path
  `get_download_url` both "succeed" (they resolve the wrong key). The render `output_url` still
  stays the project-relative `...render-file?path=working/final.mp4` — the route re-prepends the
  prefix itself. Always verify with `get_download_url` on the FULL `agent-config/...` path (must
  be non-empty; curl it for HTTP 200) BEFORE marking the render complete.
- Media generation (FAL / ElevenLabs) is billed to the agent through the GooseWorks proxies.
  `submit_render { kind: "full" }` debits **1 ad credit at row creation** — so sequence it LAST
  (render + verify a good MP4 first), and never re-submit on a guess (that double-bills). Call
  `get_ad_credits` first; the user can check `gooseworks credits`.

## Step 1 — resolve the project, source, brand

1. `get_ad_project { project_id }` → keep `brand_id`, `source_sample_id`, `name`, `status`.
2. `get_ad_template { template_id: source_sample_id }` → the source video: `media_url`,
   `recipe`, `format` (e.g. "imessage"), `extracted_script`, `how_to`, `remix_spec`.
3. Brand gate: `get_brand_kit { brand_id }`. If `researchStatus` is `complete`, REUSE it —
   never re-research. If not, run brand research first (`gooseworks fetch brand-research`,
   follow it, then `finalize_brand_research { brand_id }`) before continuing.

## Step 2 — fetch the recipe + its pack skills for the format

The video-ad format skills live in two goose-skills packs — **`video-ad-formats`** (phone-surface
HTML-mockup renders) and **`ugc-video-formats`** (GPT-image-2 + Seedance 2.0 reference-to-video).
Map the project's `format` to its recipe slug:

| format | recipe slug | renderer + atoms it drives |
| --- | --- | --- |
| `imessage` | `remix-imessage-ad-from-sample` | create-imessage-video-ad, create-imessage-mockup, stitch-videos-ffmpeg, mix-master, watch |
| `chatgpt` | `remix-chatgpt-ad-from-sample` | create-chatgpt-video-ad, create-chatgpt-mockup, render-ios-keyboard, stitch-videos-ffmpeg, watch |
| `apple-notes` | `remix-apple-notes-ad-from-sample` | create-apple-notes-video-ad, create-apple-notes-mockup, stitch-videos-ffmpeg, watch |
| `ugc-product` | `remix-ugc-product-from-sample` | create-ugc-product-video-from-refs, create-video-seedance-2-fal, create-image-gpt-image-fal, watch |
| `ugc-grwm` | `remix-ugc-grwm-from-sample` | create-ugc-grwm-video-from-refs, create-video-seedance-2-fal, create-image-gpt-image-fal, watch |
| `ugc-skincare` | `remix-ugc-skincare-from-sample` | create-ugc-skincare-demo-video-from-refs, create-video-seedance-2-fal, create-image-gpt-image-fal, watch |
| `ugc-car-confessional` | `remix-ugc-car-confessional-from-sample` | create-ugc-car-confessional-video-from-refs, create-video-seedance-2-fal, create-image-gpt-image-fal, watch |
| `ugc-walk-and-talk` | `remix-ugc-walk-and-talk-from-sample` | create-ugc-walk-and-talk-video-from-refs, create-video-seedance-2-fal, create-image-gpt-image-fal, watch |
| `ugc-street-testimonial` | `remix-ugc-street-testimonial-from-sample` | create-ugc-street-testimonial-video-from-refs, create-video-seedance-2-fal, create-image-gpt-image-fal, watch |

(photo-grid + music-video coming.) Pack skills are fetchable individually by slug but do NOT
auto-resolve dependencies (no `dependencySkills`), so `gooseworks fetch` the recipe **and** each
skill in its row — e.g. for iMessage:

```bash
for s in remix-imessage-ad-from-sample create-imessage-video-ad create-imessage-mockup \
         stitch-videos-ffmpeg mix-master watch; do gooseworks fetch "$s"; done
```

Each prints `{ content, scripts, files }`. Save each skill's scripts + files under
`/tmp/gooseworks-scripts/<slug>/` and FOLLOW the recipe's SKILL.md — it orchestrates the others.
The renderer (the `create-*-mockup` atom for the format) is a Node package —
`npm install` in its folder so its `generate.js` + Playwright resolve, and point the recorder's
`NODE_PATH` at it.

## Step 3 — prepare ALL the ingredients, then review ONCE (always, before any paid render)

This is a **review-once** flow: prepare every ingredient the video needs, show the whole set to
the user in the app, get ONE approval, then render. Never render before approval, and don't drip
ingredients out one at a time.

1. **Generate every ingredient the format needs — not just the script.** For an iMessage video
   that's typically: the **script** (the bubble thread), the **image(s)** shown in the conversation
   (one or more), and the **end card**. Richer templates add more (hook frame, background, product
   shots, music bed…). Read the recipe for the exact ingredient list. Generate the visuals NOW
   (media proxies / recipe), and `get_upload_url` each preview asset to the project folder
   `agent-config/brands/<brand_slug>/projects/<project_id>/working/review/<name>` (the same
   path-prefix rule as final publish — a bare `working/review/<name>` won't render in the panel).
   In `script_drafts`, set each ingredient's `path` to the project-relative `working/review/<name>`.
   You may ask the user a couple of clarifying questions about the generation first if the recipe
   calls for it (angle, which product, offer/code) — batch them, then prepare everything.
2. **Mirror the whole ingredient set for review** — `update_ad_project_script { project_id,
   script_drafts, script }`. `script_drafts` is a structured payload of **container-tagged
   ingredients** so the app renders each piece the right way:
   `{ format, scenes?, ingredients: [{ container, label, subtitle?, path?, text? }] }`. Each
   ingredient's `container` tells the app HOW to show it:
   - `image` (a frame shown in the video), `endcard` (the end card), `avatar` (a character
     headshot), `background` → rendered as an image tile.
   - `voice` (a voiceover clip — put the voice NAME in `subtitle`), `music` (the bed),
     `audio` → rendered as an audio player.
   - `video` (a clip) → a video player. `text` (a copy line like the CTA) → a text tile.
   - `script` / `thread` / `note` / `conversation` → the written script (or set `scenes[]`
     for the podcast shape, or pass the readable `script` string).
   `path` = `working/review/<name>` (upload the preview asset first via `get_upload_url`); `url`
   works too. **Label every ingredient** ("Hook image", "End card", "Voiceover", "Background
   music", "HER"). This writes NO render and costs NO credits — it populates the review panel.
3. **STOP and ask the user to approve the ingredients in THIS Claude Code session.** Do not render
   until they say go. If they want changes, regenerate the affected ingredient, call
   `update_ad_project_script` again, and re-ask. Only AFTER approval do Step 4.

## Step 4 — render locally, report stages, publish

1. Render per the recipe (Playwright record → ffmpeg stitch → `mix-master` audio). Generate any
   hook / background / end-card assets through the media proxies (below).
2. Open the row LAST: `submit_render { project_id, kind: "full" }` → keep `render_id`, then
   `update_render_status { render_id, status: "running" }`. The render row tracks status only
   (queued / running / complete / failed) — narrate fine-grained progress with
   `append_project_message` instead.
3. QC by watching: run the `watch` skill on the master — verify bubble/beat order + SFX, that
   the brand's product (not the source's) is shown, the end card has the real wordmark + code,
   and the duration is within ~20% of the source. For UGC/Seedance formats there are no captions
   and often no local Whisper key: extract frames with ffmpeg for the visual pass, and get the
   spoken transcript by running `fal-ai/whisper` through the SAME `fal-proxy` (upload the audio,
   pass its `get_download_url` as `audio_url`) — confirm the transcript matches the script and the
   brand name is pronounced right, with no word-repeat dysfluency, BEFORE spending the render credit.
4. Publish: `get_upload_url { target: { type: "agent", agent_id: ADS_AGENT } }` → PUT the master
   and poster **under the project folder** (see Identity's path-prefix rule) — to
   `agent-config/brands/<brand_slug>/projects/<project_id>/working/final.mp4` and
   `.../working/final-thumb.jpg`. **Always target ADS_AGENT AND use the full project-folder path**
   — a bare `working/final.mp4`, even on the right agent, 404s in the app. Verify servable:
   `get_download_url { target: ADS_AGENT, path: "agent-config/brands/<brand_slug>/projects/<project_id>/working/final.mp4" }`
   must return a non-empty URL (curl it for HTTP 200).
   Then `update_render_status { render_id, status: "complete", output_url, thumbnail_url }` where
   **output_url MUST be the durable render-file URL**
   `/api/ads/projects/<project_id>/render-file?path=working/final.mp4` (the app re-presigns it on
   every view) — NEVER a raw proxy/CDN URL (those expire). Same for `thumbnail_url`.
5. `set_final_render { project_id, render_id }` to pin it, then return the `app_url` +
   `brand_url` (from the project/links) verbatim. Never end on just "done" or a file path.

Narrate each long step in one line via `append_project_message { project_id, role: "agent",
content }` — never sit silent on a queue > 90s.

## Media generation — the GooseWorks proxies (queue loop)

Media APIs go through GooseWorks proxies with your agent token; do NOT use an SDK's default host
(your token isn't a FAL/ElevenLabs token → 401). Base = `<api_base>/api/internal/<proxy>`; pass
`?token=<api_key>&agent_id=<agent_id>` (agent_id bills the Ads agent). FAL = `fal-proxy` (+
`fal-storage-proxy` to host a local image and get a CDN URL); ElevenLabs = `elevenlabs-proxy`
(VO / music bed).

**FAL queue gotcha** (#1 waste of generations): submit returns `status_url`/`response_url` on
`queue.fal.run` (the real host, not the proxy). Polling those 401s forever — rewrite their host
to the proxy base (keep the path), re-add `?token=&agent_id=`. Only the final `*.fal.media`
image is a real public URL. Helper:

```python
import json, os, pathlib, time, requests
from urllib.parse import urlparse

def _cfg():
    c = json.loads(pathlib.Path(os.path.expanduser("~/.gooseworks/credentials.json")).read_text())
    return c["api_base"].rstrip("/"), c["api_key"], c.get("agent_id")

def _params(tok, agent):
    p = {"token": tok}
    if agent: p["agent_id"] = agent
    return p

def fal_generate(model_path, payload, timeout_s=180, poll_s=3):
    """model_path e.g. 'fal-ai/nano-banana-2/edit' (the recipe names the model).
    Returns the result image URL (a public *.fal.media CDN URL)."""
    api_base, tok, agent = _cfg()
    base = api_base + "/api/internal/fal-proxy"
    sub = requests.post(f"{base}/{model_path}", params=_params(tok, agent), json=payload).json()
    to_proxy = lambda u: base + urlparse(u).path
    status_url, response_url = to_proxy(sub["status_url"]), to_proxy(sub["response_url"])
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        st = requests.get(status_url, params=_params(tok, agent)).json()
        if st.get("status") == "COMPLETED":
            return requests.get(response_url, params=_params(tok, agent)).json()["images"][0]["url"]
        if st.get("status") in ("FAILED", "ERROR"):
            raise RuntimeError(f"FAL failed: {st}")
        time.sleep(poll_s)
    raise TimeoutError("FAL polling exceeded timeout")
```

ElevenLabs (VO / music) is the same shape against `<api_base>/api/internal/elevenlabs-proxy`
with `?token=&agent_id=`. Feed FAL a local image by storing it (`get_upload_url`) and passing its
`get_download_url` presigned URL as an `image_urls` / `audio_url` entry — this is the reliable
path. (`fal-storage-proxy` may 404 depending on the install; don't block on it — prefer the
`get_download_url` presigned URL.)

## Rules

- **MCP + ffmpeg + Playwright required** — run `gooseworks doctor` in Phase 0; stop with the
  exact fix it prints if anything is ✗.
- **Prepare ALL ingredients first** (script + every visual: image(s) + end card + whatever else
  the template needs), mirror the whole set with `update_ad_project_script`, and get the user's
  approval in-session BEFORE rendering — always (review-once).
- **submit_render LAST**; `output_url` = the durable render-file URL, never a CDN URL.
- **Verify a real, non-empty MP4** (watch it) before marking the render complete.
- **Reuse the brand** when its research is complete; never re-research.
- On a hard error (auth/quota/model/timeout) set the render `failed` with a short
  `error_message` and stop — don't ship the source unchanged.
- Always end a successful run with `app_url` + `brand_url`, verbatim.
