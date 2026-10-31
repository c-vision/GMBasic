// FILE: Basic.Connectors.ConnectorMarketplace
// Catalogo dei 52 connettori (spec MCP SYSTEM.txt §1, T0-T4) — definizioni, non installer.

namespace Basic.Connectors;

public static class ConnectorMarketplace
{
    public static IReadOnlyList<ConnectorDefinition> Catalog { get; } =
    [
        // TIER 0 — FONDAMENTALI (10)
        new("gmail", "gmail", "Gmail", "1.0", "oauth2",
            ["mail.read","mail.search","mail.send","mail.modify","mail.labels","mail.attachments"],
            ["search","read","send","reply","forward","labels","attachments"],
            "Gmail — email search/read/send with OAuth"),
        new("google-calendar", "calendar", "Google Calendar", "1.0", "oauth2",
            ["calendar.read","calendar.write","calendar.events"],
            ["events","create","update","cancel","availability","invite"],
            "Google Calendar — events and availability"),
        new("google-drive", "drive", "Google Drive", "1.0", "oauth2",
            ["drive.read","drive.write","drive.search"],
            ["search","read","write","delete","move","copy","presign"],
            "Google Drive — file storage"),
        new("github", "github", "GitHub", "1.0", "oauth2",
            ["repo.read","repo.write","issues.read","issues.write","pr.read","pr.write","actions.read","actions.write"],
            ["repository","issues","pull_requests","branches","commits","releases","actions","workflows"],
            "GitHub — repositories/issues/PR/Actions"),
        new("slack", "slack", "Slack", "1.0", "oauth2",
            ["chat.read","chat.write","channels.read","channels.write"],
            ["search","read_thread","read_channel","search_messages","list_members","send","reply","react"],
            "Slack — messaging and channels"),
        new("microsoft-365", "microsoft", "Microsoft 365", "1.0", "oauth2",
            ["mail.read","mail.send","calendar.read","calendar.write","drive.read","drive.write","teams.read"],
            ["mail.search","mail.send","calendar.events","drive.search","teams.messages","onedrive.read","sharepoint.read"],
            "Microsoft 365 — Outlook/Calendar/OneDrive/SharePoint/Teams via Graph"),
        new("web", "web", "Web", "1.0", "none",
            ["web.search","web.fetch","web.extract"],
            ["search","fetch","extract","browser","screenshot","links"],
            "Web — search/fetch/extract/browser"),
        new("http", "http", "HTTP", "1.0", "none",
            ["http.get","http.post","http.put","http.patch","http.delete"],
            ["get","post","put","patch","delete"],
            "HTTP — raw protocol with auth/headers/cookies/JSON/XML/multipart/retry"),
        new("filesystem", "filesystem", "Filesystem", "1.0", "none",
            ["fs.read","fs.write","fs.list","fs.find"],
            ["list","find","read","write","move","copy","delete","metadata","hash","diff"],
            "Filesystem — sandboxed workspace I/O"),
        new("postgres", "postgres", "PostgreSQL", "1.0", "none",
            ["db.connect","db.query","db.execute","db.schema"],
            ["connect","query","execute","transaction","schema","tables","describe"],
            "PostgreSQL — common DB API"),
        new("sqlite", "sqlite", "SQLite", "1.0", "none",
            ["db.connect","db.query","db.execute","db.schema"],
            ["connect","query","execute","transaction","schema","tables","describe"],
            "SQLite — embedded DB"),

        // TIER 1 — MOLTO UTILI (12)
        new("google-sheets", "sheets", "Google Sheets", "1.0", "oauth2",
            ["sheets.read","sheets.write"], ["read","write","append","query","insert"], "Google Sheets"),
        new("google-docs", "docs", "Google Docs", "1.0", "oauth2",
            ["docs.read","docs.write"], ["read","write","create","update"], "Google Docs"),
        new("onedrive", "onedrive", "OneDrive", "1.0", "oauth2",
            ["drive.read","drive.write"], ["list","read","write","delete","move"], "OneDrive"),
        new("sharepoint", "sharepoint", "SharePoint", "1.0", "oauth2",
            ["sharepoint.read","sharepoint.write"], ["list","read","write","search"], "SharePoint"),
        new("notion", "notion", "Notion", "1.0", "oauth2",
            ["notion.read","notion.write"], ["search","page.read","page.create","page.update","database.query","database.insert"], "Notion"),
        new("linear", "linear", "Linear", "1.0", "apikey",
            ["linear.read","linear.write"], ["issues","projects","cycles","comments","teams","labels"], "Linear"),
        new("jira", "jira", "Jira", "1.0", "oauth2",
            ["jira.read","jira.write"], ["issues","projects","comments","transitions","worklogs","sprints"], "Jira"),
        new("discord", "discord", "Discord", "1.0", "oauth2",
            ["discord.read","discord.write"], ["search","read_channel","send","reply","react"], "Discord"),
        new("telegram", "telegram", "Telegram", "1.0", "apikey",
            ["telegram.read","telegram.write"], ["search","send","reply","list_members"], "Telegram"),
        new("microsoft-teams", "teams", "Microsoft Teams", "1.0", "oauth2",
            ["teams.read","teams.write"], ["messages","channels","send","reply"], "Microsoft Teams"),
        new("dropbox", "dropbox", "Dropbox", "1.0", "oauth2",
            ["drive.read","drive.write"], ["list","read","write","delete","move","presign"], "Dropbox"),
        new("box", "box", "Box", "1.0", "oauth2",
            ["drive.read","drive.write"], ["list","read","write","delete"], "Box"),

        // TIER 2 — AUTOMAZIONE / BUSINESS (10)
        new("stripe", "stripe", "Stripe", "1.0", "apikey",
            ["stripe.read","stripe.write"], ["customers","invoices","subscriptions","payments","refunds","products"], "Stripe — HIGH_RISK"),
        new("hubspot", "hubspot", "HubSpot", "1.0", "oauth2",
            ["hubspot.read","hubspot.write"], ["contacts","companies","deals","activities","notes","tickets"], "HubSpot CRM"),
        new("salesforce", "salesforce", "Salesforce", "1.0", "oauth2",
            ["salesforce.read","salesforce.write"], ["contacts","companies","deals","activities"], "Salesforce CRM"),
        new("shopify", "shopify", "Shopify", "1.0", "apikey",
            ["shopify.read","shopify.write"], ["products","orders","customers","inventory"], "Shopify"),
        new("woocommerce", "woocommerce", "WooCommerce", "1.0", "apikey",
            ["woo.read","woo.write"], ["products","orders","customers"], "WooCommerce"),
        new("zendesk", "zendesk", "Zendesk", "1.0", "oauth2",
            ["zendesk.read","zendesk.write"], ["tickets","users","groups","search"], "Zendesk"),
        new("twilio", "twilio", "Twilio", "1.0", "apikey",
            ["twilio.read","twilio.write"], ["sms.send","call.create","messages"], "Twilio"),
        new("sendgrid", "sendgrid", "SendGrid", "1.0", "apikey",
            ["sendgrid.write"], ["send","templates","lists"], "SendGrid"),
        new("docusign", "docusign", "DocuSign", "1.0", "oauth2",
            ["docusign.read","docusign.write"], ["envelopes","templates","recipients"], "DocuSign — HIGH_RISK"),
        new("calendly", "calendly", "Calendly", "1.0", "oauth2",
            ["calendly.read","calendly.write"], ["events","invitees","scheduling"], "Calendly"),

        // TIER 3 — DEVOPS / INFRA (11)
        new("docker", "docker", "Docker", "1.0", "none",
            ["docker.read","docker.write"], ["containers","images","networks","volumes","exec"], "Docker"),
        new("kubernetes", "kubernetes", "Kubernetes", "1.0", "none",
            ["k8s.read","k8s.write"], ["pods","deployments","services","jobs","logs"], "Kubernetes"),
        new("aws", "aws", "AWS", "1.0", "apikey",
            ["aws.read","aws.write"], ["s3.list","s3.read","s3.write","lambda.invoke","ec2.list"], "AWS"),
        new("azure", "azure", "Azure", "1.0", "oauth2",
            ["azure.read","azure.write"], ["storage.list","vm.list","functions.invoke"], "Azure"),
        new("google-cloud", "gcp", "Google Cloud", "1.0", "oauth2",
            ["gcp.read","gcp.write"], ["storage.list","compute.list","functions.invoke"], "Google Cloud"),
        new("cloudflare", "cloudflare", "Cloudflare", "1.0", "apikey",
            ["cloudflare.read","cloudflare.write"], ["zones","dns","workers","r2.list"], "Cloudflare"),
        new("sentry", "sentry", "Sentry", "1.0", "apikey",
            ["sentry.read"], ["errors","traces","metrics","logs","alerts"], "Sentry — observability"),
        new("grafana", "grafana", "Grafana", "1.0", "apikey",
            ["grafana.read"], ["dashboards","metrics","alerts"], "Grafana"),
        new("prometheus", "prometheus", "Prometheus", "1.0", "none",
            ["prometheus.read"], ["query","metrics","alerts"], "Prometheus"),
        new("gitlab", "gitlab", "GitLab", "1.0", "oauth2",
            ["gitlab.read","gitlab.write"], ["repositories","issues","merge_requests","pipelines"], "GitLab"),
        new("bitbucket", "bitbucket", "Bitbucket", "1.0", "oauth2",
            ["bitbucket.read","bitbucket.write"], ["repositories","issues","pull_requests"], "Bitbucket"),

        // TIER 4 — AI / KNOWLEDGE (9)
        new("openai", "openai", "OpenAI", "1.0", "apikey",
            ["ai.chat","ai.embed"], ["chat","embed","models","files"], "OpenAI"),
        new("anthropic", "anthropic", "Anthropic", "1.0", "apikey",
            ["ai.chat"], ["chat","models"], "Anthropic"),
        new("openrouter", "openrouter", "OpenRouter", "1.0", "apikey",
            ["ai.chat"], ["chat","models"], "OpenRouter"),
        new("huggingface", "huggingface", "Hugging Face", "1.0", "apikey",
            ["hf.read"], ["models","datasets","inference"], "Hugging Face"),
        new("weights-biases", "wandb", "Weights & Biases", "1.0", "apikey",
            ["wandb.read","wandb.write"], ["runs","artifacts","sweeps"], "Weights & Biases"),
        new("qdrant", "qdrant", "Qdrant", "1.0", "none",
            ["vector.read","vector.write"], ["insert","search","delete","collections","metadata"], "Qdrant"),
        new("chroma", "chroma", "Chroma", "1.0", "none",
            ["vector.read","vector.write"], ["insert","search","delete","collections"], "Chroma"),
        new("elasticsearch", "elasticsearch", "Elasticsearch", "1.0", "none",
            ["search.read","search.write"], ["search","index","delete","indices"], "Elasticsearch"),
        new("neo4j", "neo4j", "Neo4j", "1.0", "none",
            ["graph.read","graph.write"], ["query","insert","traverse","collections"], "Neo4j"),
    ];

    // F4 marketplace remoto (SYSTEM.txt §34): se GMBASIC_CONNECTOR_MARKETPLACE_URL è impostata, fetcha JSON remoto (versionato) e mergia con Catalog statico
    public static IReadOnlyList<ConnectorDefinition> CatalogWithRemote
    {
        get
        {
            var remoteUrl = Environment.GetEnvironmentVariable("GMBASIC_CONNECTOR_MARKETPLACE_URL");
            if (string.IsNullOrWhiteSpace(remoteUrl)) return Catalog;
            try
            {
                var helper = new CommonLib.Http.HttpClientHelper();
                var opts = new CommonLib.Http.HttpOptions { Method = "GET" };
                var res = helper.PerformRequestAsync(remoteUrl, opts).GetAwaiter().GetResult();
                if (!res.IsSuccess || string.IsNullOrWhiteSpace(res.Body)) return Catalog;
                using var doc = System.Text.Json.JsonDocument.Parse(res.Body);
                var list = new List<ConnectorDefinition>(Catalog);
                if (doc.RootElement.ValueKind == System.Text.Json.JsonValueKind.Array)
                {
                    foreach (var el in doc.RootElement.EnumerateArray())
                    {
                        var name = el.TryGetProperty("name", out var n) ? n.GetString() : null;
                        if (string.IsNullOrWhiteSpace(name) || list.Any(c => c.Name.Equals(name, StringComparison.OrdinalIgnoreCase))) continue;
                        var kind = el.TryGetProperty("kind", out var k) ? k.GetString() ?? name : name;
                        var ver = el.TryGetProperty("version", out var v) ? v.GetString() ?? "1.0" : "1.0";
                        list.Add(new ConnectorDefinition(name, kind, name, ver, "none", [kind], [kind], $"Remote {name}"));
                    }
                }
                return list;
            }
            catch { return Catalog; }
        }
    }

    public static ConnectorDefinition? Get(string name) =>
        Catalog.FirstOrDefault(c => c.Name.Equals(name, StringComparison.OrdinalIgnoreCase));

    public static IReadOnlyList<ConnectorDefinition> Search(string? capability = null, string? kind = null)
    {
        IEnumerable<ConnectorDefinition> q = Catalog;
        if (!string.IsNullOrWhiteSpace(capability))
            q = q.Where(c => c.Capabilities.Any(cap => cap.Contains(capability, StringComparison.OrdinalIgnoreCase))
                || c.Tools.Any(t => t.Contains(capability, StringComparison.OrdinalIgnoreCase)));
        if (!string.IsNullOrWhiteSpace(kind))
            q = q.Where(c => c.Kind.Equals(kind, StringComparison.OrdinalIgnoreCase));
        return q.ToList();
    }
}
