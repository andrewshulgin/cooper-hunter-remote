const argparse = require('argparse');
const http = require('http');
const url = require('url');
const WebSocket = require('ws');

const CooperHunterEsp8266Client = require('./index').CooperHunterEsp8266Client;

const defaults = {
    caddr: '192.168.218.10',
    cport: 1234,
    lport: 8080,
    ws: '/ws'
};

const args = (
    function () {
        const argParser = new argparse.ArgumentParser();
        argParser.addArgument(['-a', '--caddr']);
        argParser.addArgument(['-p', '--cport']);
        argParser.addArgument(['-l', '--lport']);
        argParser.addArgument(['-w', '--ws']);
        return argParser.parseArgs();
    }
)();

const CADDR = process.env.CADDR || args.caddr || defaults.caddr;
const CPORT = process.env.CPORT || args.cport || defaults.cport;
const LPORT = process.env.LPORT || args.lport || defaults.lport;
const WS = process.env.WS || args.ws || defaults.ws;

let current_data = null;

const server = http.createServer(function (req, res) {
    const pathname = url.parse(req.url).pathname;
    if (pathname.indexOf('\0') !== -1) {
        res.writeHead(404);
        res.end();
        return;
    }

    if (pathname === '/favicon.ico') {
        res.writeHead(204);
        res.end();
        return;
    }

    if (req.method === 'POST' || req.method === 'PUT') {
        let recvBuffer = '';
        req.on('data', chunk => {
            recvBuffer += chunk;
        });
        req.on('end', () => {
            try {
                const data = JSON.parse(recvBuffer.toString());
                client.setParameters(data);
                const body = JSON.stringify(data);
                res.writeHead(200, {
                    'Content-Length': Buffer.byteLength(body),
                    'Content-Type': 'application/json'
                });
                res.end(body);
            } catch (e) {
                res.writeHead(400);
                res.end();
            }
        });
        return;
    }

    const body = JSON.stringify(current_data);

    res.writeHead(200, {
        'Content-Length': Buffer.byteLength(body),
        'Content-Type': 'application/json'
    });
    res.end(body);
}).listen(LPORT);

const wsServer = new WebSocket.Server({
    server: server,
    path: WS
});

const client = new CooperHunterEsp8266Client(
    CADDR, CPORT, (data) => {
        current_data = data;
        broadcast(JSON.stringify(current_data));
    }
);

client.connect();

['SIGINT', 'SIGTERM'].forEach(signal => process.on(signal, () => {
    client.close(() => server.close(() => process.exit(0)));
}));

wsServer.on('connection', ws => {
    ws.isAlive = true;
    ws.on('pong', () => {
        ws.isAlive = true;
    });
    ws.on('message', (data) => {
        try {
            client.setParameters(JSON.parse(data));
        } catch (e) {
        }
    });
    ws.pingInterval = setInterval(() => {
        if (ws.isAlive === false) {
            clearInterval(ws.pingInterval);
            return ws.terminate();
        }
        ws.isAlive = false;
        ws.ping();
    }, 30000);
    ws.send(JSON.stringify(current_data));
});

const broadcast = data => {
    wsServer.clients.forEach(function each(client) {
        if (client.readyState === WebSocket.OPEN) {
            client.send(data);
        }
    });
};
