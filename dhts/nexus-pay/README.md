# ⬡ Nexus-Pay

**AI Unified Proxy & Billing Manager**

Nexus-Pay is a standalone Development Helper Tool (DHT) for the Nexus Systems ecosystem. It helps you manage multiple AI provider accounts, track usage/tokens, and provides a unified proxy with automatic failover to OpenAI.

## Features

- **Usage Dashboard:** Track how many tokens and dollars you've spent across providers.
- **Unified Proxy:** An OpenAI-compatible endpoint (`localhost:3999/v1/chat/completions`) that routes to your configured accounts.
- **Intelligent Routing:** Automatically tries your preferred/prepaid accounts first.
- **Failover:** If one provider fails or is exhausted, it automatically tries the next one, with OpenAI as the final fallback.
- **Billing Visibility:** Keep track of which accounts are paid and how much budget remains.

## Quick Start

### 1. Install Dependencies
```bash
bun install
```

### 2. Add Your Accounts
```bash
# Add an OpenRouter account with a $50 budget
bun run src/cli.ts add openrouter "My Main OpenRouter" YOUR_API_KEY 50

# Add OpenAI as a fallback
bun run src/cli.ts add openai "My OpenAI Fallback" YOUR_API_KEY 20
```

### 3. List Accounts
```bash
bun run src/cli.ts list
```

### 4. Start the Proxy
```bash
bun run src/index.ts
```

### 5. Use the Proxy
Point your AI-powered tools (or `curl`) to the Nexus-Pay proxy:
```bash
curl http://localhost:3999/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "gpt-4",
    "messages": [{"role": "user", "content": "Hello!"}]
  }'
```

## Supported Providers
- [x] OpenAI
- [x] OpenRouter
- [x] Anthropic (Routing only, usage tracking coming soon)
- [ ] Google Gemini (Coming soon)
- [ ] GitHub Models (Coming soon)

---
*Internal Nexus Systems DHT — Not for public distribution.*
