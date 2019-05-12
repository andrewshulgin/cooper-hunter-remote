const net = require('net');

const modes = ['auto', 'cool', 'dry', 'fan', 'heat'];
const swing = [
    'last',
    'auto',
    'up',
    'middle_up',
    'middle',
    'middle_down',
    'down',
    'down_auto',
    null,
    'middle_auto',
    null,
    'up_auto'
];

function serialize(data) {
    return [
        data.power ? '1' : '0',
        modes.indexOf(data.mode).toString(),
        data.temp.toString(),
        data.fan === 'auto' ? 0 : data.fan.toString(),
        data.turbo ? '1' : '0',
        data.xfan ? '1' : '0',
        data.light ? '1' : '0',
        data.sleep ? '1' : '0',
        data.swing_auto ? '1' : '0',
        swing.indexOf(data.swing).toString()
    ].join(',') + '\r\n';
}

function deserialize(data) {
    const split = data.toString().split(',');
    return {
        'power': split[0] === '1',
        'mode': modes[parseInt(split[1])],
        'temp': parseInt(split[2]),
        'fan': split[3] === '0' ? 'auto' : parseInt(split[3]),
        'turbo': split[4] === '1',
        'xfan': split[5] === '1',
        'light': split[6] === '1',
        'sleep': split[7] === '1',
        'swing_auto': split[8] === '1',
        'swing': swing[parseInt(split[9])]
    };
}

class CooperHunterEsp8266Client {
    constructor(address, port, callback, reconnect = true) {
        this.address = address;
        this.port = port;
        this.client = new net.Socket();
        this.callback = callback;
        this.reconnect = reconnect;
        this.connected = false;
    }

    connect() {
        this.client.connect(this.port, this.address, () => {
            this.connected = true;
        });
        this.client.on('data', data => {
            this.callback(deserialize(data));
        });
        this.client.on('close', () => {
            this.connected = false;
            if (this.reconnect)
                setTimeout(this.connect, 1000);
        });
    }

    close() {
        this.client.close();
    }

    setParameters(parameters) {
        if (this.connected) {
            this.client.write(serialize(parameters));
        }
    }
}

exports.CooperHunterEsp8266Client = CooperHunterEsp8266Client;
