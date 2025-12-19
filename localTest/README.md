# Sonos Web Remote

Eine einfache Weboberfläche zur Steuerung eines Sonos-Lautsprechers.

## Installation

1. Node.js installieren (v14 oder höher)
2. In das `web`-Verzeichnis wechseln:
   ```bash
   cd web
   ```
3. Dependencies installieren:
   ```bash
   npm install
   ```
4. Server starten:
   ```bash
   npm start
   ```
5. Im Browser öffnen: http://localhost:3000

## Funktionen

- Play/Pause Toggle
- Lautstärke erhöhen (+5)
- Lautstärke verringern (-5)

## Technische Details

- Express.js Server
- Direkter UPnP/SOAP-Zugriff auf Sonos
- Responsives Web-Interface
- Fehlerbehandlung mit visueller Rückmeldung

## Konfiguration

Die Sonos-IP-Adresse kann in `server.js` angepasst werden:

```js
const SONOS_IP = '192.168.2.228';  // IP des Sonos-Lautsprechers
```