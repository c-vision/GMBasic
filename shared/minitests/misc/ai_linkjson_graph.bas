' EXPECT: get:hello world|out:[{"From":"n1.md","To":"assente.md","Alias":"rotto","Heading":null,"Resolved":false},{"From":"n1.md","To":"n2.md","Alias":"vedi","Heading":null,"Resolved":true}]|dang: 1|json:{"nodes":[{"id":"n1.md","title":"n1","frontmatter":{"title":["n1"],"tags":["alpha","beta"]}},{"id":"n2.md","title":"n2","frontmatter":{"title":["n2"]}}],"edges":[{"from":"n1.md","to":"n2.md","alias":"vedi"}],"dangling":["n1.md \u2192 assente.md"]}
' MiniTest — AI.LINKJSON su vault POPOLATO (nodes con id/title/frontmatter, edges, dangling),
' oracolo IL (LinkManager.ToJson). Setup = pattern di misc/note_domain.bas (che usa la
' cartella work/ esistente per entrambe le basi di path).
IMPORT AI

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
    DIM v AS STRING = GMBWORK$ + "/linkjson-vault"
    IF IO.DirExists(v) = 0 THEN MKDIR v
    ' Root come LETTERALE: `ParseAiVaultCreateArgs` usa ParseNameAsString, che converte un
    ' identificatore nel letterale del suo NOME (`v` → "v") — passare la variabile rendeva
    ' il test non deterministico (vault registrato da run precedenti con root diverse).
    AI VAULT CREATE "ljtest", GMBWORK$ + "/linkjson-vault"
    AI VAULT INDEX "ljtest"
    AI NOTE CREATE "n1.md", "hello world", "alpha,beta"
    AI NOTE CREATE "n2.md", "seconda nota"
    ' Forma FUNZIONE (vault esplicito): la statement `AI NOTE LINK` passa "" come vault e
    ' usa il brain ATTIVO → con vault non attivo il link non viene registrato (verificato).
    AI.NOTELINK("ljtest", "n1.md", "n2.md", "vedi")
    AI.NOTELINK("ljtest", "n1.md", "assente.md", "rotto")
    PRINT "get:"; AI.NOTEGET("ljtest", "n1.md")
    PRINT "out:"; AI.LINKSOUT("ljtest", "n1.md")
    PRINT "dang:"; AI.LINKDANGLING("ljtest")
    PRINT "json:"; AI.LINKJSON("ljtest")
END SUB
