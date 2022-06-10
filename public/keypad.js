let ws;
let audio =new AudioContext();
let vs = [];

// Randomly add a data point every 500ms
const memory = new TimeSeries();

function createTimeline() {
  var chart = new SmoothieChart({
    millisPerPixel: 75,
    maxValue: 327680,
    minValue: 0,
    interpolation: 'step',
    tooltip: true,
    labels: { precision: 0 },
  });
  chart.addTimeSeries(memory, { strokeStyle: 'rgba(0, 255, 0, 1)', fillStyle: 'rgba(0, 255, 0, 0.2)', lineWidth: 4 });
  chart.streamTo(document.getElementById("chart"), 500);
}

function quiet() {
  vs.forEach(v => {
    try {
      v.stop();
    } catch (e) {
    }
  });
  vs = [];
}

function beep(vol, freq, duration){
  quiet();
  const v = audio.createOscillator();
  const u = audio.createGain();
  v.onended = (e) => {
    vs = vs.filter(item => item !== v);
  };
  v.connect(u);
  v.frequency.value = freq;
  v.type = "sine";
  u.connect(audio.destination);
  u.gain.value=vol*0.01;
  vs.push(v);
  v.start(audio.currentTime);
  v.stop(audio.currentTime+duration*0.001);
}

function emblinken(data) {
  let blinkenText = '';
  let separator = data.lastIndexOf('|');
  if (separator === -1) {
    return data;
  }
  let text = data.substring(0, separator);
  let flashingCharactersHex = data.substring(separator+1);
  let flashingCharacters = 0;
  try {
    flashingCharacters = parseInt(flashingCharactersHex, 16);
  } catch (e) {
    console.error(`failed to parse hex number after | character: ${data}`, e);
    return data;
  }

  let flashing = false;
  for (let i = 0; i < text.length; i++) {
    let shouldFlash = flashingCharacters & (1 << i);
    if ((shouldFlash && flashing) ||
        (!shouldFlash && !flashing)) {
      // No change, just append
      blinkenText += text[i];
    } else if (shouldFlash) {
      flashing = true;
      blinkenText += `<span class="blink">${text[i]}`;
    } else {
      flashing = false;
      blinkenText += `</span>${text[i]}`;
    }
  }
  if (flashing) {
    blinkenText += '</span>';
  }
  return blinkenText;
}

function onLoad(callback) {
  createTimeline();
  const cookie = document.cookie;
  let host = localStorage.getItem('host');
  if (host === null) {
    host = window.location.host;
  }

  let secureStr = localStorage.getItem('secure');
  if (secure !== null) {
    secure = secureStr === 'true';
  }
  document.getElementById('host').value = host;
  document.getElementById('secure').checked = secure;
}

function toggleConnection(callback) {
  if (!ws) {
    document.getElementById('display').innerText = 'Connecting...';
    const host = document.getElementById('host').value;
    const secure = document.getElementById('secure').checked;
    localStorage.setItem('host', host);
    localStorage.setItem('secure', secure);
    const url = `${secure ? 'wss' : 'ws'}://${host}/ws`;
    ws = new WebSocket(url);
    ws.onopen = function () {
      document.getElementById('display').innerText = 'Socket open';
      // Send a single message, needed at the moment for the server side to
      // get a handle to the socket.
      const vkpToken = document.getElementById('vkpToken').value;
      ws.send(`A${vkpToken}`);
      if (callback) {
        callback();
      }
    };
    ws.onmessage = onKeypadMessage;
    ws.onclose = function () {
      // websocket is closed.
      //alert("Connection is closed...");
      document.getElementById('display').innerText = 'Server closed socket';
      document.getElementById('connect').innerText = 'Connect';
      ws = null;
    };
    document.getElementById('connect').innerText = 'Disconnect';
  } else {
    document.getElementById('display').innerText = 'Closing socket...';
    ws.close();
    document.getElementById('connect').innerText = 'Connect';
    ws = null;
  }
}

function onKeypadMessage(evt) {
    console.log(evt.data);
    switch (evt.data[0]) {
      case 'D':
        keypadDisplay(evt.data);
      break;
      case 'L':
        keypadLeds(evt.data);
      break;
      case 'B':
        keypadBacklight(evt.data);
      break;
      case 'T':
        keypadTone(evt.data);
      break;
      case 'A':
        keypadAudio(evt.data);
      break;
      case 'F':
        console.info(`Auth result: ${evt.data}`)
        if (evt.data[1] === 'T') {
          document.getElementById('display').innerText = 'Authenticated';
        } else {
          document.getElementById('display').innerText = 'Authentication failed';
        }
      break;
      case 'G': {
          const data = JSON.parse(evt.data.slice(1));
          const message = `${new Date().toLocaleTimeString()}: ${data.freeHeap?.toLocaleString()}`;
          document.getElementById('uptime').innerText = `${Math.round(data.uptime / 1000).toLocaleString()} seconds`;
          document.getElementById('memory').innerText = `${data.freeHeap?.toLocaleString()} bytes`;
          document.getElementById('lastLog').innerText = message;
          
          let longLog = document.getElementById('log').innerText.split('\n');
          longLog.slice(longLog.length - 100)
          longLog.push(message);
          document.getElementById('log').innerText = longLog.join('\n');
          memory.append(new Date().getTime(), data.freeHeap);
      }
        break;
      default:
        console.error(`Unknown command: ${evt.data}`);
      break;
    }
}

function savePassword() {
  // TODO: https://developer.mozilla.org/en-US/docs/Web/API/CredentialsContainer/store
  if (window.PasswordCredential) {
    var c = new PasswordCredential(e.target);
    return navigator.credentials.store(c);
  } else {
    return Promise.resolve(profile);
  }
}

function keypadDisplay(text) {
    let processed = emblinken(text.substring(1));
    document.getElementById('display').innerHTML = processed;
}

function keypadLeds(data) {
  setKeypadLedState('keypadLedPower', data[1]);
  setKeypadLedState('keypadLedWarn', data[2]);
  setKeypadLedState('keypadLedAlarm', data[3]);
}

function setKeypadLedState(id, state) {
  const led = document.getElementById(id);
  switch (state) {
    case '0':
      led.classList.remove('blink');
      led.classList.add('ledDisabled');
      break;
    case '1':
      led.classList.add('blink');
      led.classList.remove('ledDisabled');
      break;
    case '2':
      led.classList.remove('blink');
      led.classList.remove('ledDisabled');
      break;
  }
}

function keypadTone(data) {
  switch(data[1]) {
    case '1':
      beep(1, 1000, 30*1000);
      break;
    case '0':
      quiet();
  }
}

function keypadBacklight(data) {
  console.log(`Backlight ${data}`);
  const display = document.getElementById('display');
  if (data[1] == '0') {
    display.classList.remove('displayLight');
    display.classList.add('displayDark');
  } else {
    display.classList.remove('displayDark');
    display.classList.add('displayLight');
  }
}

function keypadAudio(data) {
  document.getElementById('audio').innerText = data;
}

function keyClick(character) {
  if (!ws) {
    toggleConnection(() => { keyClick(character); });
  } else {
    beep( 1, 1000, 100);
    ws.send(`K${character}`);
  }
}
function uploadFile(type) {
  let f;
  let command;
  if (type === 'cert') {
    f = certupload.files[0];
    command = 'C';
  } else {
    f = keyupload.files[0];
    command = 'R';
  }
  f.text()
   .then(t => {
    ws.send(`${command}${t}`);
   });
}

function setVkpToken() {
  let f;
  let command;
  const t = document.getElementById('newVkpToken').value;
  ws.send(`S${t}`);
}

function setAlarmToken() {
  let f;
  let command;
  const t = document.getElementById('newAlarmToken').value;
  ws.send(`T${t}`);
}


function setSerialNumber() {
  let f;
  let command;
  const snt1 = document.getElementById('newSerialNumber1').value;
  const snt2 = document.getElementById('newSerialNumber2').value;
  const snt3 = document.getElementById('newSerialNumber3').value;
  const sn1 = Number.parseInt(snt1);
  const sn2 = Number.parseInt(snt2);
  const sn3 = Number.parseInt(snt3);
  if (isNaN(sn1) || sn1 < 0 || sn1 > 255) {
    document.getElementById('display').innerText = 'Must be number from 0 to 255';
  } else if (isNaN(sn2) || sn2 < 0 || sn2 > 255) {
    document.getElementById('display').innerText = 'Must be number from 0 to 255';
  } else if (isNaN(sn3) || sn3 < 0 || sn3 > 255) {
    document.getElementById('display').innerText = 'Must be number from 0 to 255';
  } else {
    const str = String.fromCharCode(sn1, sn2, sn3);
    ws.send(`N${str}`);
  }
}

function reset() {
  ws.send(`E`);
}

function setResponseDelay() {
  const delay = document.getElementById('vkpResponseDelayMS').value;
  ws.send(`D${delay}`);
}