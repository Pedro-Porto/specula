# 🛡️ Specula

**Specula** is a lightweight, terminal-based monitoring and control system for Linux virtual machines.  
It provides real-time metrics (CPU, memory, storage) and allows executing, managing, and inspecting commands across multiple VMs through a secure socket protocol — all without external dependencies.

---

## 🧠 Overview

Specula consists of two components:

- **Agent** — runs on each VM, exposing metrics and command execution endpoints over TCP.
- **Controller** — connects to multiple agents, displays system stats (like `top`), and manages distributed commands interactively or in batch.

Both communicate over **raw TCP sockets** using a simple **framed text protocol**.

---

## 📡 Architecture

```

+-----------------+          +------------------+
|   Controller    | <------> |   Agent (VM 1)   |
|                 | <------> |   Agent (VM 2)   |
| Terminal UI     | <------> |   Agent (VM N)   |
+-----------------+          +------------------+

```

Each agent listens on a port and requires an authentication token (`MON_TOKEN`).  
All communication follows the message framing format below.

---

## 🧾 Message Format

Each message is a **frame** consisting of:

```

<LENGTH>\n<PAYLOAD>

```

- `LENGTH`: decimal number of bytes in `<PAYLOAD>` (no padding).
- `\n`: newline delimiter.
- `<PAYLOAD>`: the command or response body.

Example:

```

12
PING

```

and the response:

```

6
PONG

```

This ensures messages are parsed correctly even if multiple arrive in the same TCP stream.

---

## 🔐 Authentication

When a controller connects, it **must** authenticate first:

**Controller → Agent**
```

AUTH <token>

```

**Agent → Controller**
```

OK agent

```
or
```

ERR unauthorized

```

If authentication fails, the agent closes the connection immediately.

---

## ⚙️ Supported Commands

### 1. System Status (multi-VM summary)

**Command name:** `STATUS`

Displays metrics for all connected VMs, similar to the `top` command.  
Each line shows one VM with CPU, memory, and storage stats.

**Controller → Agent**
```

STATUS

```

**Agent → Controller**
```

STATUS cpu=23% mem=1.2GiB/4.0GiB disk=9.8GiB/30GiB

```

The controller aggregates one line per VM.

---

### 2. Individual Metrics

**Command:** `CPU`, `MEM`, `DISK`

**Controller → Agent**
```

CPU

```

**Agent → Controller**
```

CPU 23%

```

Similarly for memory and disk:

```

MEM 1.2GiB/4.0GiB
DISK 9.8GiB/30GiB

```

---

### 3. List Connected Machines

**Command:** `LIST`

**Controller (internal)** — requests list of connected agents and assigns IDs automatically.

**Controller → Operator output example:**
```

ID   HOST           PORT   STATUS
1    10.0.0.11      60119  connected
2    10.0.0.12      60119  connected
3    10.0.0.13      60119  offline

```

This list is managed locally by the controller — agents do not need to respond.

---

### 4. Execute Commands (Batch or Manual)

#### (a) Batch Execution

Executes a shell command on **all connected agents**.

**Controller → Each Agent**
```

EXEC <command>

```

**Agent → Controller**
```

EXEC_END code=<exit_code>

```

The controller then summarizes results:

```

## BATCH SUMMARY

Success (code=0): vm1, vm3, vm4
Failed  (code=1): vm2

```

#### (b) Manual Execution (Single Machine)

Executes on a specific VM ID from `LIST`.

**Controller → Agent[id=2]**
```

EXEC <command>

```

**Agent → Controller**
(streamed output)
```

OUT <sid> <chunk>
OUT <sid> <chunk>
OUT_END <sid> <exit_code>

```

The controller prints chunks live on the terminal.

---

### 5. Start Background Process

**Controller → Agent**
```

START <command>

```

**Agent → Controller**
```

START_OK pid=<pid>

```

If command fails:
```

ERR start_failed

```

---

### 6. Kill Process

**Controller → Agent**
```

KILL <pid> [force]

```

**Agent → Controller**
```

KILL_OK

```
or
```

ERR kill_failed

```

---

### 7. Tail Log File

**Controller → Agent**
```

TAIL <path>

```

**Agent → Controller (streaming)**
```

OUT <sid> <chunk>
OUT_END <sid> <exit_code>

```

Used for `tail -F`-style real-time log inspection.

---

### 8. Heartbeat

**Controller → Agent**
```

PING

```

**Agent → Controller**
```

PONG

```

Used to verify agent health.

---

### 9. Disconnect

**Controller → Agent**
```

BYE

```

**Agent → Controller**
```

OK bye

```

The connection is then closed gracefully.

---

## 💬 Message Direction Summary

| Command      | From → To        | Description |
|---------------|------------------|--------------|
| `AUTH`        | Controller → Agent | Authentication |
| `PING` / `PONG` | Controller ↔ Agent | Heartbeat |
| `STATUS`      | Controller → Agent | VM metrics summary |
| `CPU`, `MEM`, `DISK` | Controller → Agent | Individual metric |
| `LIST`        | Controller (local) | Lists connected VMs |
| `EXEC`        | Controller → Agent | Execute command |
| `START`       | Controller → Agent | Start process in background |
| `KILL`        | Controller → Agent | Terminate process |
| `TAIL`        | Controller → Agent | Stream log output |
| `BYE`         | Controller → Agent | Graceful disconnect |

---

## 📋 Frame Examples

### Example 1 — `STATUS`
```

Controller → Agent:
7
STATUS

Agent → Controller:
62
STATUS cpu=15% mem=3.2GiB/8.0GiB disk=9.8GiB/30GiB

```

### Example 2 — `EXEC` with output
```

Controller → Agent:
15
EXEC /usr/bin/uptime

Agent → Controller:
22
OUT 1  14:32:10 up  5:02,  load average: 0.02, 0.04, 0.00
13
OUT_END 1 0

```

---

## 🔧 Development Guidelines

- Each side must **always** respect the frame format (`<len>\n<payload>`).
- Each command must return **one and only one** response frame (unless it is a stream).
- Streamed commands (`EXEC`, `TAIL`) **must** use:
  - `OUT <sid> <chunk>` for partial data
  - `OUT_END <sid> <exit_code>` to finalize
- Exit codes follow standard Linux convention (`0` = success).
- Agents must remain stateless — controllers handle all session logic and IDs.
- Any `ERR` response indicates failure; the connection remains open unless explicitly closed.

---

## 🧩 Example Workflow

1. Controller authenticates to all VMs.  
2. Operator presses `s` or runs `STATUS` → aggregated “top-style” table appears.  
3. Operator executes:
   - `EXEC uptime` (batch mode) → summarized success/failure table.  
   - `EXEC 2 ps aux` (manual mode) → runs only on VM with ID=2.  
4. Operator tails `/var/log/syslog` on VM 1.  
5. When finished, sends `BYE`.

---

## 🤝 Collaboration Notes

Developers implementing **controller** or **agent** should:
- Use the **exact same framing and message naming**.
- Test interoperability using simple netcat sessions (`nc`) for debugging.
- Avoid assumptions about line breaks inside payloads — rely strictly on frame length.

---

## 🧩 Example Command Table for Team Reference

| Command | Args | Response | Streamed | Description |
|----------|------|-----------|-----------|--------------|
| `AUTH` | `<token>` | `OK agent` / `ERR unauthorized` | No | Authenticate client |
| `PING` | – | `PONG` | No | Heartbeat |
| `STATUS` | – | `STATUS cpu=.. mem=.. disk=..` | No | Combined VM metrics |
| `CPU` | – | `CPU <percent>` | No | CPU only |
| `MEM` | – | `MEM <used>/<total>` | No | Memory only |
| `DISK` | – | `DISK <used>/<total>` | No | Disk only |
| `EXEC` | `<cmd>` | `OUT ... / OUT_END ...` | Yes | Execute command |
| `START` | `<cmd>` | `START_OK pid=<pid>` | No | Run process in background |
| `KILL` | `<pid> [force]` | `KILL_OK` / `ERR ...` | No | Terminate process |
| `TAIL` | `<path>` | `OUT ... / OUT_END ...` | Yes | Follow log file |
| `BYE` | – | `OK bye` | No | Disconnect |

---

*Specula — the eye and the hand over your VMs.*

