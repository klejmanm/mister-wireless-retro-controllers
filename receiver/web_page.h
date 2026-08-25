#ifndef WEB_PAGE_H
#define WEB_PAGE_H

#include <pgmspace.h>

const char HTML_DIAGNOSTIC[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>MiSTer Retro RX</title>
  <style>
    body { font-family: monospace; background: #0f0f13; color: #00ff66; text-align: center; padding: 15px; margin: 0; }
    .container { max-width: 450px; margin: auto; }
    .card { background: #181820; border: 1px solid #00ff66; border-radius: 8px; padding: 15px; margin-bottom: 15px; text-align: left; }
    h2 { margin-top: 0; color: #00aaff; border-bottom: 1px solid #333; padding-bottom: 5px; font-size: 1.1em; }
    p { margin: 6px 0; }
    .val { font-size: 1.1em; font-weight: bold; color: #fff; }
    .status-online { color: #00ff66; font-weight: bold; }
    .status-offline { color: #ff0055; font-weight: bold; }
    progress { width: 100%; height: 18px; margin-top: 5px; }
    .btn-reset { background: #ff0055; color: #fff; border: none; padding: 8px 12px; border-radius: 4px; cursor: pointer; font-family: monospace; font-weight: bold; margin-top: 10px; width: 100%; }
    .btn-reset:hover { background: #cc0044; }
    .btn-freq { background: #00aaff; color: #000; border: none; padding: 6px 10px; border-radius: 4px; cursor: pointer; font-family: monospace; font-weight: bold; margin: 4px; }
    .btn-freq:hover { background: #0088cc; }
  </style>
</head>
<body>
  <div class="container">
    <h2>MiSTer Retro RX (Single Host)</h2>
    
    <div class="card">
      <h2>PERFORMANCE & POWER</h2>
      <p>CPU Clock: <b><span id="cpu_freq" class="val">--</span> MHz</b></p>
      <button class="btn-freq" onclick="setFreq(240)">L.L (240MHz)</button>
      <button class="btn-freq" onclick="setFreq(160)">M.L (160MHz)</button>
      <button class="btn-freq" onclick="setFreq(80)">H.L (80MHz)</button>
    </div>

    <div class="card">
      <h2>SYSTEM & USB STATUS</h2>
      <p>USB HID Device: <b>MiSTer-S1 Spinner</b></p>
      <p>USB State: <span id="usb_status" class="val">--</span></p>
      <p>USB Output Rate: <span id="usb_hz" class="val">0</span> Hz</p>
      <p>mDNS Address: <a id="mdns_link" href="#" target="_blank" style="color:#fff; font-weight:bold;">http://<span id="mdns_name">--</span></a></p>
    </div>

    <div class="card">
      <h2>TRANSMITTER LINK (NVS)</h2>
      <p>Paired MAC: <span id="tx_mac" class="val">UNBOUND</span></p>
      <p>Link State: <span id="tx_status">WAITING</span></p>
      <p>Sleep In: <span id="tx_timeout" class="val">--</span> s</p>
      <p>Packets RX: <span id="tx_pkts" class="val">0</span></p>
      <p>ADC Raw: <span id="tx_val" class="val">0</span> / 4095</p>
      <progress id="tx_bar" value="0" max="4095"></progress>
      <p>Fire Button: <span id="tx_btn" class="val">RELEASED</span></p>
      <button class="btn-reset" onclick="resetBinding()">Reset Permanent Pairing</button>
    </div>
  </div>

  <script>
    function resetBinding() {
      if (confirm("Clear permanent NVS pairing and wait for new transmitter?")) {
        fetch('/reset').then(() => location.reload());
      }
    }

    function setFreq(f) {
      if (confirm("Set CPU clock to " + f + " MHz and reboot the receiver?")) {
        fetch('/setfreq?val=' + f).then(() => {
          setTimeout(() => { location.reload(); }, 3000);
        });
      }
    }

    setInterval(() => {
      fetch('/json').then(r => r.json()).then(d => {
        let usbSt = document.getElementById('usb_status');
        usbSt.innerText = d.usb_mounted ? 'CONNECTED' : 'DISCONNECTED';
        usbSt.className = d.usb_mounted ? 'status-online' : 'status-offline';
        
        document.getElementById('usb_hz').innerText = d.usb_hz;
        document.getElementById('cpu_freq').innerText = d.cpu_freq;

        document.getElementById('mdns_name').innerText = d.mdns_name;
        document.getElementById('mdns_link').href = 'http://' + d.mdns_name;

        document.getElementById('tx_mac').innerText = d.bound ? d.mac : 'UNBOUND';
        
        let st = document.getElementById('tx_status');
        st.innerText = d.online ? 'ONLINE' : (d.bound ? 'LOST / TIMEOUT' : 'WAITING FOR TX');
        st.className = d.online ? 'status-online' : 'status-offline';

        document.getElementById('tx_timeout').innerText = d.online ? d.sleep_timeout_sec : '--';
        document.getElementById('tx_pkts').innerText = d.packets;
        document.getElementById('tx_val').innerText = d.value;
        document.getElementById('tx_bar').value = d.value;
        document.getElementById('tx_btn').innerText = (d.buttons & 1) ? 'PRESSED' : 'RELEASED';
      });
    }, 150);
  </script>
</body>
</html>
)rawliteral";

#endif