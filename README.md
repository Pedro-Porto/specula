# 🛡️ Specula

**Specula** is a lightweight, terminal-based monitoring and control system for Linux systems.  
It provides real-time system metrics (CPU, memory, disk usage) and remote command execution capabilities through a TCP socket protocol.

---

## 🧠 Overview

Specula consists of two components:

- **Agent** — connects to the controller, reports system metrics and executes commands remotely.
- **Controller** — acts as a TCP server, accepts agent connections, displays system stats, and manages command execution.

Both communicate over **raw TCP sockets** using a **length-prefixed message framing protocol**.

---

## 📡 Architecture

```
+-----------------+          +------------------+
|   Controller    | <------> |   Agent (VM 1)   |
|   (TCP Server)  | <------> |   Agent (VM 2)   |
| Terminal UI     | <------> |   Agent (VM N)   |
+-----------------+          +------------------+
```

- **Controller** listens on port 60119 and accepts incoming agent connections
- **Agents** connect to the controller with automatic reconnection on connection loss
- All communication requires authentication using a hardcoded token (`supersecret`)

---

## 🧾 Message Protocol

### Frame Format
Each message is a **length-prefixed frame** consisting of:

```
<LENGTH>\n<PAYLOAD>
```

- `LENGTH`: decimal number of bytes in `<PAYLOAD>` (no padding)
- `\n`: newline delimiter  
- `PAYLOAD`: the actual command and data

**Example:**
```
4
PING
```

**Response:**
```
4
PONG
```

### Message Structure
The payload follows this format:
```
<COMMAND> <ARGUMENTS>
```

Where `COMMAND` is the operation name and `ARGUMENTS` contains optional parameters or data.

---

## 🔐 Authentication

When an agent connects to the controller, it **must** authenticate first:

**Agent → Controller:**
```
4
AUTH supersecret
```

**Controller → Agent (success):**
```
8
OK agent
```

**Controller → Agent (failure):**
```
15
ERR unauthorized
```

If authentication fails, the connection is closed. All subsequent commands require authentication.

---

## ⚙️ Implemented Commands

### 1. Heartbeat
**Purpose:** Keep-alive mechanism and connection health check

**Controller → Agent:**
```
4
PING
```

**Agent → Controller:**
```
4
PONG
```

The controller automatically broadcasts PING every 2 seconds to all connected agents.

---

### 2. System Status
**Purpose:** Get comprehensive system metrics from agents

**Controller → Agent:**
```
6
STATUS
```

**Agent → Controller:**
```
45
STATUS cpu=23.5% mem=1048576/4194304 disk=102400/307200
```

Response format: `cpu=<percent>% mem=<used_kb>/<total_kb> disk=<used_kb>/<total_kb>`

The controller aggregates this data and can display it in a dashboard format.

---

### 3. Command Execution
**Purpose:** Execute shell commands remotely on agents

**Controller → Agent:**
```
25
EXEC id=123 monitor=1
whoami
```

**Parameters:**
- `id`: Unique command identifier  
- `monitor`: `1` to stream output, `0` for execution-only

**Agent Response (with monitoring):**
```
14
EXEC_OUT id=123
root

```
```
17
EXEC_DONE id=123 code=0
```

**Agent Response (without monitoring):**
```
17
EXEC_DONE id=123 code=0
```

The controller tracks command execution state and can aggregate results from multiple agents.

---

### 4. Graceful Disconnect

**Controller → Agent:**
```
3
BYE
```

**Agent → Controller:**
```
6
OK bye
```

Used to cleanly terminate connections.

---

## 💬 Command Reference

| Command      | Direction         | Purpose | Authentication Required |
|--------------|-------------------|---------|-------------------------|
| `AUTH`       | Agent → Controller | Authenticate connection | No |
| `PING`       | Controller → Agent | Health check | No |
| `PONG`       | Agent → Controller | Health check response | No |
| `STATUS`     | Controller → Agent | Request system metrics | Yes |
| `EXEC`       | Controller → Agent | Execute shell command | Yes |
| `EXEC_OUT`   | Agent → Controller | Stream command output | Yes |
| `EXEC_DONE`  | Agent → Controller | Command completion | Yes |
| `BYE`        | Controller → Agent | Graceful disconnect | No |

---

## Implementation Details

### Socket Communication
- **Protocol:** Raw TCP sockets with custom framing
- **Port:** 60119 (hardcoded)
- **Connection Model:** Controller acts as server, agents as clients
- **Reconnection:** Agents automatically reconnect with exponential backoff
- **Threading:** Each connection runs in its own thread for message processing

### Message Processing
- **Frame Parsing:** Length-prefixed messages prevent stream corruption
- **Command Dispatch:** Asynchronous handler system using function callbacks
- **Authentication State:** Per-connection authentication tracking
- **Error Handling:** Graceful error responses with connection preservation

### System Monitoring
- **CPU Usage:** Calculated from `/proc/stat` parsing
- **Memory Usage:** Read from `/proc/meminfo` (used/total in KB)
- **Disk Usage:** Uses `statvfs()` system call for filesystem stats
- **Update Frequency:** Real-time on request (no periodic polling)

---

## Running Specula

### Building
```bash
make 
```

### Controller (Server)
```bash
make run-controller
```
- Starts TCP server on port 60119
- Provides interactive CLI for managing agents
- Displays real-time system stats from connected agents
- Broadcasts PING messages every 2 seconds for health monitoring

### Agent (Client)  
```bash
make run-agent
```
- Connects to controller at `127.0.0.1:60119`
- Authenticates using hardcoded token `supersecret`
- Reports system metrics on request
- Executes remote commands and streams output
- Automatically reconnects on connection loss

### Interactive Commands
Once connected, the controller CLI supports:
- **Status monitoring:** View aggregated system stats from all agents
- **Command execution:** Run shell commands on connected agents
- **Real-time output:** Stream command output as it executes


*Specula — the eye and the hand over your VMs.*

