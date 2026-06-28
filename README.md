# XML to JSON Web Service Demo

## Task Description
Create a web service (over TCP/IP). the service allows to convert XML to JSON and vice versa.

Supported requests:
* convert the source data

Application specification:
* communication: any convenient protocol (HTTP, JSON-prc, SOAP, etc.)
* async server architecture
* third party framewroks and libraries are preferable
* C++11 is allowed
* focus on vertical and functional scalability
* no GUI
* daemon (service) mode, optional
* configuration via config file, optional
* logging (any solution)
* unit tests (any suitable framework) required

Estimate work before realization.

Desired outcome:
* instructions with information on supported platforms and build tests
* source code
* tests

## Setup

### Make sure relevant MCP servers are connected
The Coder and Planner agents reference `context7/*` (and Coder also references `github/*`) in their tools list. If you haven't installed the Context7 MCP server, those tool references show as "Unknown tool." Install Context7 via Copilot's MCP/extensions search (`@mcp context7`), and the GitHub MCP server similarly if you want `github/*` to resolve.

### Use Agent mode, talk to the Orchestrator
Switch the chat mode to **Agent** (not Ask or Plan — those modes restrict file-editing tools and can prevent delegation from working). Select the **Orchestrator** agent from the agent picker, and just type your request normally, e.g. "Add dark mode to the app."

### What happens next
The Orchestrator:
- Calls **Planner** to get an implementation plan with file assignments
- Parses the plan into phases, grouping non-overlapping-file tasks to run in parallel and overlapping-file tasks sequentially
- Spawns **Coder** and/or **Designer** as subagents for each phase (in parallel where safe)
- Waits for each phase, reports progress, and verifies the final result

The Orchestrator's own tool list is deliberately minimal (`read/readFile`, `agent`, `memory`) — it has no edit tools, by design, so it's structurally forced to delegate rather than write code itself.

### Known rough edges (from the gist's comment thread)
- If Orchestrator suddenly says "I don't have file editing tools" and stops, it's usually `chat.customAgentInSubagent.enabled` not being on, or it silently fell back to a model/mode where delegation breaks.
- Use Command Palette → `GitHub Copilot: Open Chat Debug View` to confirm subagent calls are actually firing (you should see `tool/runSubagent-Coder`, etc., in the logs).
- A few users got a 400 "unsupported model / prompt caching" error specifically when Orchestrator was set to a Claude Sonnet model in non-Insiders VS Code — switching the Orchestrator's model fixed it for at least one person.
- One commenter forked this into a more elaborate variant if you want to see an extended take, but the original four-agent setup is the minimal version described above.

## Tools
AI framework based on [Ultralight Orchestration](https://gist.github.com/burkeholland/0e68481f96e94bbb98134fa6efd00436)