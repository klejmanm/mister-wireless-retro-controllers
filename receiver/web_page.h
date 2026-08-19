#ifndef WEB_PAGE_H
#define WEB_PAGE_H

#include <pgmspace.h>

const char HTML_DIAGNOSTIC[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Paddle/Spinner Receiver</title>
  <style>
    body { font-family: monospace; background: #0f0f13; color: #00ff66; text-align: center; padding: 15px; margin: 0; }
    .container { max-width: 600px; margin: auto; }
    .card { background: #181820; border: 1px solid #00ff66; border-radius: 8px; padding: 15px; margin-bottom: 15px; text-align: left; }
    h2 { margin-top: 0; color: #00aaff; border-bottom: 1px solid #333; padding-bottom: 5px; }
    .val { font-size: 1.1em; font-weight: bold; color: #fff; }
    .status-online { color: #00ff66; font-weight: bold; }
    .status-offline { color: #ff0055; font-weight: bold; }
    .status-unbound { color: #888888; font-style: italic; }
    progress { width: 100%; height: 16px; margin-top: 5px; }
    .btn-reset { background: #ff0055; color: #fff; border: none; padding: 8px 12px; border-radius: 4px; cursor: pointer; font-family: monospace; font-weight: bold; margin-top: 10px; }
    .btn-freq { background: #00aaff; color: #000; border: none; padding: 6px 10px; border-radius: 4px; cursor: pointer; font-family: monospace; font-weight: bold; margin: 4px; }
    .btn-freq:hover { background: #0088cc; }
  </style>
</head>
<body>
  <div class="container">
    <h1>Paddle/Spinner Receiver</h1>
    
    <div class="card">
      <h2>Performance & Power</h2>
      <p>CPU Clock: <b><span id="cpu_freq" class="val">--</span> MHz</b></p>
      <button class="btn-freq" onclick="setFreq(240)">L.L (240MHz)</button>
      <button class="btn-freq" onclick="setFreq(160)">M.L (160MHz)</button>
      <button class="btn-freq" onclick="setFreq(80)">H.L (80MHz)</button>
      <p style="font-size: 0.8em; color: #aaa; margin-top: 8px;">L.L = Low Latency (Warm) | H.L = High Latency (Cool)</p>
    </div>

    <div class="card">
      <h2>USB Host Connection</h2>
      <p>USB Status: <span id="usb_status" class="val">--</span></p>
      <p>USB HID Device: <b>MiSTer-S1 Spinner</b></p>
      <p>USB Output Rate: <span id="usb_hz" class="val">0</span> Hz (reports/sec)</p>
    </div>

    <div class="card">
      <h2>Wireless Host Status</h2>
      <p>Wi-Fi Channel: <b>6 (Fixed SoftAP)</b> | IP: <b>192.168.4.1</b></p>
      <p>Host MAC: <b><span id="host_mac" class="val">--</span></b></p>
      <button class="btn-reset" onclick="resetSlots()">Reset Controller Bindings</button>
    </div>
    
    <div class="card">
      <h2>Paddle 1 (Player 1)</h2>
      <p>MAC: <span id="s0_mac" class="val">NONE</span></p>
      <p>Status: <span id="s0_status">--</span> | Packets: <span id="s0_pkts" class="val">0</span></p>
      <p>Raw ADC: <span id="s0_val" class="val">0</span> / 4095</p>
      <progress id="s0_bar" value="0" max="4095"></progress>
      <p>Fire Button: <span id="s0_btn" class="val">OFF</span></p>
    </div>

    <div class="card">
      <h2>Paddle 2 (Player 2)</h2>
      <p>MAC: <span id="s1_mac" class="val">NONE</span></p>
      <p>Status: <span id="s1_status">--</span> | Packets: <span id="s1_pkts" class="val">0</span></p>
      <p>Raw ADC: <span id="s1_val" class="val">0</span> / 4095</p>
      <progress id="s1_bar" value="0" max="4095"></progress>
      <p>Fire Button: <span id="s1_btn" class="val">OFF</span></p>
    </div>

    <div class="card">
      <h2>Spinner</h2>
      <p>MAC: <span id="s2_mac" class="val">NONE</span></p>
      <p>Status: <span id="s2_status">--</span> | Packets: <span id="s2_pkts" class="val">0</span></p>
      <p>Last Rel Delta: <span id="s2_val" class="val">0</span></p>
      <p>Fire Button: <span id="s2_btn" class="val">OFF</span></p>
    </div>
  </div>

  <script>
    function resetSlots() {
      if (confirm("Clear all dynamic bindings?")) {
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
        document.getElementById('host_mac').innerText = d.host_mac;
        document.getElementById('cpu_freq').innerText = d.cpu_freq;
        
        let usbSt = document.getElementById('usb_status');
        usbSt.innerText = d.usb_mounted ? 'READY / CONNECTED' : 'DISCONNECTED / SUSPENDED';
        usbSt.className = d.usb_mounted ? 'status-online' : 'status-offline';
        document.getElementById('usb_hz').innerText = d.usb_hz;

        for(let i=0; i<3; i++) {
          let slot = d.slots[i];
          document.getElementById('s' + i + '_mac').innerText = slot.bound ? slot.mac : 'UNBOUND';
          
          let st = document.getElementById('s' + i + '_status');
          if(!slot.bound) {
            st.innerText = 'WAITING';
            st.className = 'status-unbound';
          } else {
            st.innerText = slot.online ? 'ONLINE' : 'OFFLINE';
            st.className = slot.online ? 'status-online' : 'status-offline';
          }

          document.getElementById('s' + i + '_pkts').innerText = slot.packets;
          document.getElementById('s' + i + '_val').innerText = slot.value;
          if(i < 2) document.getElementById('s' + i + '_bar').value = slot.value;
          document.getElementById('s' + i + '_btn').innerText = (slot.buttons & 1) ? 'PRESSED' : 'RELEASED';
        }
      });
    }, 200);
  </script>
</body>
</html>
)rawliteral";

#endif