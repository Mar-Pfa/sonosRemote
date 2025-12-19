const express = require('express');
const fetch = require('node-fetch');
const path = require('path');

const app = express();
const port = 3000;

const SONOS_IP = '192.168.2.228';
const SONOS_PORT = 1400;

// Serve static files from public directory
app.use(express.static('public'));

// Helper function to send SOAP requests to Sonos
async function sonosRequest(service, action, body = '') {
    const response = await fetch(`http://${SONOS_IP}:${SONOS_PORT}${service}`, {
        method: 'POST',
        headers: {
            'Content-Type': 'text/xml',
            'SOAPACTION': `urn:schemas-upnp-org:service:${action}`,
            'Connection': 'close'
        },
        body
    });

    if (!response.ok) {
        throw new Error(`Sonos request failed: ${response.statusText}`);
    }

    return response;
}

// Play/Pause toggle
app.post('/sonos/playpause', async (req, res) => {
    try {
        // First get current transport state
        const stateResponse = await sonosRequest(
            '/MediaRenderer/AVTransport/Control',
            'AVTransport:1#GetTransportInfo',
            '<?xml version="1.0" encoding="utf-8"?>' +
            '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">' +
            '<s:Body>' +
            '<u:GetTransportInfo xmlns:u="urn:schemas-upnp-org:service:AVTransport:1">' +
            '<InstanceID>0</InstanceID>' +
            '</u:GetTransportInfo>' +
            '</s:Body>' +
            '</s:Envelope>'
        );

        const stateText = await stateResponse.text();
        const isPlaying = stateText.includes('PLAYING');

        // Then send play or pause based on current state
        const action = isPlaying ? 'Pause' : 'Play';
        await sonosRequest(
            '/MediaRenderer/AVTransport/Control',
            `AVTransport:1#${action}`,
            '<?xml version="1.0" encoding="utf-8"?>' +
            '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">' +
            '<s:Body>' +
            `<u:${action} xmlns:u="urn:schemas-upnp-org:service:AVTransport:1">` +
            '<InstanceID>0</InstanceID>' +
            `<Speed>1</Speed>` +
            `</u:${action}>` +
            '</s:Body>' +
            '</s:Envelope>'
        );

        res.sendStatus(200);
    } catch (error) {
        console.error('Error controlling playback:', error);
        res.sendStatus(500);
    }
});

// Volume up
app.post('/sonos/volumeup', async (req, res) => {
    try {
        // First get current volume
        const volumeResponse = await sonosRequest(
            '/MediaRenderer/RenderingControl/Control',
            'RenderingControl:1#GetVolume',
            '<?xml version="1.0" encoding="utf-8"?>' +
            '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">' +
            '<s:Body>' +
            '<u:GetVolume xmlns:u="urn:schemas-upnp-org:service:RenderingControl:1">' +
            '<InstanceID>0</InstanceID>' +
            '<Channel>Master</Channel>' +
            '</u:GetVolume>' +
            '</s:Body>' +
            '</s:Envelope>'
        );

        const volumeText = await volumeResponse.text();
        const currentVolume = parseInt(volumeText.match(/<CurrentVolume>(\d+)<\/CurrentVolume>/)?.[1] || '0');
        const newVolume = Math.min(currentVolume + 5, 100); // Increase by 5, max 100

        // Set new volume
        await sonosRequest(
            '/MediaRenderer/RenderingControl/Control',
            'RenderingControl:1#SetVolume',
            '<?xml version="1.0" encoding="utf-8"?>' +
            '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">' +
            '<s:Body>' +
            '<u:SetVolume xmlns:u="urn:schemas-upnp-org:service:RenderingControl:1">' +
            '<InstanceID>0</InstanceID>' +
            '<Channel>Master</Channel>' +
            `<DesiredVolume>${newVolume}</DesiredVolume>` +
            '</u:SetVolume>' +
            '</s:Body>' +
            '</s:Envelope>'
        );

        res.sendStatus(200);
    } catch (error) {
        console.error('Error increasing volume:', error);
        res.sendStatus(500);
    }
});

// Volume down
app.post('/sonos/volumedown', async (req, res) => {
    try {
        // First get current volume
        const volumeResponse = await sonosRequest(
            '/MediaRenderer/RenderingControl/Control',
            'RenderingControl:1#GetVolume',
            '<?xml version="1.0" encoding="utf-8"?>' +
            '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">' +
            '<s:Body>' +
            '<u:GetVolume xmlns:u="urn:schemas-upnp-org:service:RenderingControl:1">' +
            '<InstanceID>0</InstanceID>' +
            '<Channel>Master</Channel>' +
            '</u:GetVolume>' +
            '</s:Body>' +
            '</s:Envelope>'
        );

        const volumeText = await volumeResponse.text();
        const currentVolume = parseInt(volumeText.match(/<CurrentVolume>(\d+)<\/CurrentVolume>/)?.[1] || '0');
        const newVolume = Math.max(currentVolume - 5, 0); // Decrease by 5, min 0

        // Set new volume
        await sonosRequest(
            '/MediaRenderer/RenderingControl/Control',
            'RenderingControl:1#SetVolume',
            '<?xml version="1.0" encoding="utf-8"?>' +
            '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">' +
            '<s:Body>' +
            '<u:SetVolume xmlns:u="urn:schemas-upnp-org:service:RenderingControl:1">' +
            '<InstanceID>0</InstanceID>' +
            '<Channel>Master</Channel>' +
            `<DesiredVolume>${newVolume}</DesiredVolume>` +
            '</u:SetVolume>' +
            '</s:Body>' +
            '</s:Envelope>'
        );

        res.sendStatus(200);
    } catch (error) {
        console.error('Error decreasing volume:', error);
        res.sendStatus(500);
    }
});

// Serve the main page
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

app.listen(port, () => {
    console.log(`Sonos web remote running at http://localhost:${port}`);
});