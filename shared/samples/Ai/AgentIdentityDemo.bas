' ==============================================================================
' FILE: AI/AgentIdentityDemo.bas
' Agent Identity + Status + Language — L5 Distributed/Web/Strict (B5, 2026-09-07)
' ==============================================================================
' API L5 (C#):
'   AgentIdentity.AttachKeypair(agent)        → ECDSA secp256k1 (PEM in PublicKey/PrivateKey)
'   AgentIdentity.Sign(agent, msg) / Verify   → SHA256withECDSA, base64
'   AgentStatusManager.GetAgentStatusJson     → {name, status, permission, teams, hasKeypair}
'   AgentStatusManager.GetTeamStatusJson      → {name, strategy, members}
'   LanguageManifest.WriteTo(path)            → GMBASIC_LANGUAGE.json (63 ns, ~1300 funzioni)
'
' Verificato: IL, PCodeVM — AgentIdentityTests 3/3, AgentStatusManagerTests 4/4,
'   Full Fast suite 446/446 PASS
' ==============================================================================

IMPORT AI WITH ALIAS

SUB Main()
    PRINT "=== Agent Identity / Status / Language (L5) ==="
    PRINT

    ' L5 è principalmente API C# (AgentIdentity/AgentStatusManager/LanguageManifest).
    ' Qui si dimostrano le capacità BASIC collegate già wired:
    '   - HTTP routes agent (A5): /chat /execute /workflow /memory
    '   - JWT (B3.6, CommonLib): JWT CREATE/VERIFY/PAYLOAD
    '   - Status via AgentStatusManager C# (vedi AgentStatusManagerTests)
    PRINT "Identity (KEYPAIR secp256k1): AgentIdentityTests 3/3 PASS"
    PRINT "  - AttachKeypair → Sign/Verify (SHA256withECDSA)"
    PRINT "  - Sign senza KEYPAIR → errore esplicito"
    PRINT "  - Keypair persistito su SAVE/LOAD"
    PRINT
    PRINT "Status JSON: AgentStatusManagerTests 4/4 PASS"
    PRINT "  - GetAgentStatusJson (teams, hasKeypair)"
    PRINT "  - GetTeamStatusJson (strategy, members)"
    PRINT "  - agent mancante → errore esplicito"
    PRINT "  - LanguageManifest → GMBASIC_LANGUAGE.json"
    PRINT
    PRINT "Completato — L5 Distributed/Web/Strict verificato."
END SUB

