/**
 * WebSocket → TCP proxy
 *
 * Each browser tab that connects here becomes one TCP client on the C server.
 * Messages typed in the browser are forwarded to the C server over TCP.
 * Anything the C server sends back is forwarded to the browser over WebSocket.
 *
 * Usage:  node proxy.js
 * Listens : ws://localhost:8080  (browser connects here)
 * Forwards: tcp://127.0.0.1:5000 (C server)
 */

'use strict';

const WebSocket = require('ws');
const net = require('net');

const WS_PORT = 8080;
const TCP_HOST = '127.0.0.1';
const TCP_PORT = 5000;

const wss = new WebSocket.Server({ port: WS_PORT });
console.log(`[proxy] WebSocket server listening on ws://localhost:${WS_PORT}`);
console.log(`[proxy] Forwarding each connection → tcp://${TCP_HOST}:${TCP_PORT}`);

wss.on('connection', (ws, req) => {
    const remoteIP = req.socket.remoteAddress;
    console.log(`[proxy] Browser connected from ${remoteIP}`);

    /* Open one dedicated TCP connection for this browser tab */
    const tcp = new net.Socket();

    tcp.connect(TCP_PORT, TCP_HOST, () => {
        console.log(`[proxy] TCP link open for ${remoteIP}`);
        ws.send(JSON.stringify({ type: 'status', text: 'Connected to chat server' }));
    });

    tcp.on('error', (err) => {
        console.error(`[proxy] TCP error (${remoteIP}): ${err.message}`);
        ws.send(JSON.stringify({ type: 'error', text: `Server error: ${err.message}` }));
        ws.close();
    });

    tcp.on('data', (data) => {
        /* Forward raw server text to the browser as a JSON chat message */
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: 'message', text: data.toString() }));
        }
    });

    tcp.on('close', () => {
        console.log(`[proxy] TCP closed for ${remoteIP}`);
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: 'status', text: 'Disconnected from server' }));
            ws.close();
        }
    });

    /* Browser → server */
    ws.on('message', (raw) => {
        try {
            const { text } = JSON.parse(raw);
            if (text && tcp.writable) {
                /* C server expects lines terminated with \n */
                tcp.write(text.endsWith('\n') ? text : text + '\n');
            }
        } catch {
            /* plain string fallback */
            if (tcp.writable) tcp.write(raw + '\n');
        }
    });

    ws.on('close', () => {
        console.log(`[proxy] Browser disconnected (${remoteIP})`);
        tcp.destroy();
    });

    ws.on('error', (err) => {
        console.error(`[proxy] WS error (${remoteIP}): ${err.message}`);
        tcp.destroy();
    });
});
