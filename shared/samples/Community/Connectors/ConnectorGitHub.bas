' FILE: Connectors/ConnectorGitHub.bas
' Esempio pratico GitHub (T0) — repository / issues / PR / actions via AI CONNECTOR
' Senza conoscere REST endpoint / pagination / MIME / OAuth refresh — il connettore astrae

IMPORT AI

SUB Main()
    PRINT "=== GitHub Connector Demo ==="

    ' 1. Load dal marketplace (github = OAuth2, 8 tool)
    AI CONNECTOR LOAD "github"
    PRINT "LOAD github OK"

    DIM st$ AS STRING = AI.CONNECTORSTATUS("github")
    PRINT "github status: "; LEFT(st$, 300)

    ' 2. Ricerca connettori GitHub-like
    DIM found$ AS STRING = AI.CONNECTORSEARCH("github")
    PRINT "SEARCH github len="; LEN(found$)

    ' 3. Auth (simulato — in prod OAuth PKCE via ConnectorAuth.bas)
    AI CONNECTOR SECRET SET "github", "api_key", "ghp_demo_github_token"
    AI CONNECTOR AUTH "github", "{""access_token"":""g_pat_github"",""scope"":""repo""}"
    PRINT "AUTH github Bearer stored"

    ' 4. Chiamate tool astratti (F0 stub — F1 implementa GitHub REST con pagination + OAuth)
    DIM repo$ AS STRING = AI.CONNECTORCALL("github", "repository", "{""name"":""c-vision/GMBasic""}")
    PRINT "repository: "; LEFT(repo$, 300)

    DIM issues$ AS STRING = AI.CONNECTORCALL("github", "issues", "{""repo"":""GMBasic"",""state"":""open""}")
    PRINT "issues: "; LEFT(issues$, 300)

    DIM pr$ AS STRING = AI.CONNECTORCALL("github", "pull_requests", "{""repo"":""GMBasic""}")
    PRINT "pull_requests: "; LEFT(pr$, 300)

    DIM commits$ AS STRING = AI.CONNECTORCALL("github", "commits", "{""repo"":""GMBasic"",""branch"":""main""}")
    PRINT "commits: "; LEFT(commits$, 300)

    DIM actions$ AS STRING = AI.CONNECTORCALL("github", "actions", "{""repo"":""GMBasic""}")
    PRINT "actions: "; LEFT(actions$, 300)

    ' 5. Risorse e prompt (MCP resources/read, prompts/get)
    DIM res$ AS STRING = AI.CONNECTORRESOURCES("github")
    PRINT "RESOURCES: "; LEFT(res$, 200)
    DIM prom$ AS STRING = AI.CONNECTORPROMPTS("github")
    PRINT "PROMPTS: "; LEFT(prom$, 200)

    ' 6. Policy HIGH_RISK: git_push / production_deploy richiedono approval
    AI CONNECTOR POLICY SET "github", "git_push", "approve"
    DIM pol$ AS STRING = AI.CONNECTORPOLICYGET("github", "git_push")
    PRINT "POLICY git_push="; pol$

    PRINT "=== GitHub Demo OK (F0 stub, F1 = GitHub REST reale) ==="
END SUB
