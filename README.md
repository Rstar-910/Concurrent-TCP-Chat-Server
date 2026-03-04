# Concurrent TCP Chat Server

A multi-client TCP chat server written in **C** (using `select()` for I/O multiplexing) with a **Node.js WebSocket proxy** and a **browser-based chat UI**.

---

## Architecture

```
Browser Tab(s)          Node.js Proxy           C Server
┌─────────────┐         ┌──────────────┐        ┌──────────────┐
│  index.html │◄──WS───►│  proxy.js    │◄──TCP──►│multi_server.c│
│  :browser   │         │  :8080       │        │  :5000       │
└─────────────┘         └──────────────┘        └──────────────┘
```

Each browser tab that connects opens **one dedicated TCP connection** to the C server via the proxy. The server assigns each connection a numeric client ID.

---

## Project Structure

```
Concurrent TCP Chat Server/
├── multi_server.c       # C chat server (TCP port 5000)
├── multi_client.c       # Optional terminal-based C client
├── README.md
└── frontend/
    ├── index.html       # Browser chat UI
    ├── proxy.js         # WebSocket ↔ TCP bridge (port 8080)
    └── package.json
```

---

## Prerequisites

| Tool | Purpose |
|---|---|
| GCC | Compile the C server |
| Ubuntu WSL | Linux environment to run the C server |
| Node.js | Run the WebSocket proxy |
| Google Chrome | Open the chat UI |

> **Windows users:** Run `wsl --install -d Ubuntu` in PowerShell to get a full Ubuntu environment, then `sudo apt install -y gcc` inside it.

---

## Setup & Running

### 1 — Compile and start the C server (Ubuntu WSL terminal)

```bash
cd "/mnt/c/Users/Dell/Desktop/Tech Odyssey/Concurrent TCP Chat Server"
gcc multi_server.c -o server
./server
```

Expected output:
```
Server started on port 5000 (max 50 clients)
Send a message: <id>:<message>
```

### 2 — Install Node dependencies (first time only)

```bash
cd "/mnt/c/Users/Dell/Desktop/Tech Odyssey/Concurrent TCP Chat Server/frontend"
# or via PowerShell:
cd "C:\Users\Dell\Desktop\Tech Odyssey\Concurrent TCP Chat Server\frontend"
npm install
```

### 3 — Start the WebSocket proxy (PowerShell or any terminal)

```powershell
cd "C:\Users\Dell\Desktop\Tech Odyssey\Concurrent TCP Chat Server\frontend"
node proxy.js
```

Expected output:
```
[proxy] WebSocket server listening on ws://localhost:8080
[proxy] Forwarding each connection → tcp://127.0.0.1:5000
```

### 4 — Open the chat UI in Chrome

```powershell
start chrome "C:\Users\Dell\Desktop\Tech Odyssey\Concurrent TCP Chat Server\frontend\index.html"
```

Or just double-click `index.html` and open with Chrome.  
Click **Connect** — your Client ID will appear in the sidebar automatically.

Open multiple tabs to simulate multiple users.

---

## Messaging Reference

### From the Browser

| Action | Result |
|---|---|
| Type in **main chatbox** + Enter | 📢 Broadcast → all other clients |
| Sidebar → **Direct Message** (enter ID + message) | 🔒 Sent to that client only |
| Sidebar → **Message to Server** | ✉️ Printed on server console only, not relayed |

### From the Server Console

| Input | Result |
|---|---|
| `*:hello everyone` | 📢 Broadcast to **all** connected clients |
| `all:hello everyone` | Same as above |
| `2:hello` | Sends only to **Client 2** |

---

## Message Types (Visual)

| Style | Meaning |
|---|---|
| **Purple bubble (right)** | Your own sent messages |
| **Dark bubble (left)** | Message received from the server |
| **Blue border bubble 🔒** | Private DM received |
| **Amber bubble 📢** | Broadcast message |
| *Italic centre text* | System events (connected, disconnected, etc.) |

---

## C Server Features

- **`select()`-based** I/O multiplexing — no thread per client
- **`SO_REUSEADDR`** — prevents "Address already in use" on restart
- **Max client guard** — rejects connections beyond 50 with a message
- **`send_all()`** — handles partial TCP sends correctly
- **Client-to-client DM** — server relays `id:message` packets between clients
- **Broadcast** — `*:message` from any client or the server console fans out to all
- **Full IP + port logging** — every connect, disconnect, and message is logged with IP and port

---

## Verify Connections

```powershell
# Check active connections on both ports
netstat -ano | findstr "5000"
netstat -ano | findstr "8080"
```

Look for `ESTABLISHED` entries — one per connected browser tab.

---

## Stopping

| Component | How to stop |
|---|---|
| C server | `Ctrl+C` in the WSL terminal |
| Node proxy | `Ctrl+C` in its terminal |
| Browser clients | Click **Disconnect** or close the tab |
