# Source and provenance — `browser-skill`

- **Upstream**: <https://github.com/Tencent/BrowserSkill> — file `skill/SKILL.md`
- **License**: **MIT** (repository `LICENSE`, © Tencent). Vendored by retaining attribution in
  this file; the skill body is unmodified.
- **Vendored on**: 2026-09-17 from branch `main`
  (`https://raw.githubusercontent.com/Tencent/BrowserSkill/main/skill/SKILL.md`).
- **Upstream sha256** (as downloaded, 15126 bytes):
  `cba2fdf48df55fffa7ff3e456395d50d53f384aad15e50e8329609a1fa95aecf`
- **Only local adaptation**: the YAML frontmatter `description: |` block scalar was collapsed to a
  single quoted line so that `common/AI/SkillIndex.cs` → `ReadDescription()` extracts it into
  `skills/SKILLS_PROMPT.md`. With a block scalar and no following key in the frontmatter, that
  regex returns an **empty** description (index entry with no summary). Skill body: untouched.
- **Runtime requirement**: the skill drives the **`bsk` CLI + browser extension** (installed
  outside this repository, Chromium-based browser):
  `curl -fsSL https://raw.githubusercontent.com/Tencent/BrowserSkill/main/install.sh | sh`
  then `bsk install-skill --list --json`. The GMBasic copy is a *vendored* copy of the same
  `SKILL.md`, so the agent archive exposes it without installing it into another harness.
- **Updating**: re-download `skill/SKILL.md` and re-apply the frontmatter normalization above.
