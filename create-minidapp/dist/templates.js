"use strict";
/**
 * Built-in MiniDapp templates
 * Each template generates a complete, runnable MiniDapp project
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.TEMPLATES = void 0;
exports.listTemplates = listTemplates;
// ─── BASIC TEMPLATE ────────────────────────────────────────────────────────────
const basicTemplate = {
    name: 'basic',
    description: 'Minimal MiniDapp — balance display and send form',
    files: [
        {
            path: 'dapp.conf',
            content: `{
  "name": "{{NAME}}",
  "icon": "icon.png",
  "version": "1.0.0",
  "description": "{{DESCRIPTION}}",
  "browser": "index.html"
}`,
        },
        {
            path: 'index.html',
            content: `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>{{NAME}}</title>
  <script src="mds.js"></script>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: system-ui, sans-serif; background: #0f0f0f; color: #f0f0f0; padding: 24px; }
    h1 { font-size: 1.5rem; margin-bottom: 16px; color: #00d4aa; }
    .card { background: #1a1a1a; border-radius: 12px; padding: 20px; margin-bottom: 16px; }
    .label { font-size: 0.75rem; color: #888; text-transform: uppercase; letter-spacing: 0.05em; margin-bottom: 4px; }
    .value { font-size: 1.5rem; font-weight: 600; }
    input { width: 100%; background: #111; border: 1px solid #333; border-radius: 8px; padding: 10px 14px; color: #f0f0f0; font-size: 1rem; margin-bottom: 10px; outline: none; }
    input:focus { border-color: #00d4aa; }
    button { background: #00d4aa; color: #000; border: none; border-radius: 8px; padding: 12px 24px; font-size: 1rem; font-weight: 600; cursor: pointer; width: 100%; }
    button:disabled { opacity: 0.5; cursor: not-allowed; }
    .status { margin-top: 12px; font-size: 0.875rem; color: #888; min-height: 20px; }
    .status.ok  { color: #00d4aa; }
    .status.err { color: #ff6b6b; }
  </style>
</head>
<body>
  <h1>{{NAME}}</h1>

  <div class="card">
    <div class="label">Balance</div>
    <div class="value" id="balance">Loading…</div>
  </div>

  <div class="card">
    <div class="label">Send Minima</div>
    <input id="address" placeholder="Recipient address (0x…)" />
    <input id="amount"  placeholder="Amount" type="number" min="0" step="any" />
    <button id="sendBtn" onclick="send()">Send</button>
    <div class="status" id="status"></div>
  </div>

  <script>
    MDS.init(msg => {
      if (msg.event === 'inited') {
        loadBalance();
      }
    });

    function loadBalance() {
      MDS.cmd('balance', res => {
        const minima = res.response.find(b => b.token.name === 'Minima');
        document.getElementById('balance').textContent =
          minima ? minima.confirmed + ' MINIMA' : '0 MINIMA';
      });
    }

    function send() {
      const address = document.getElementById('address').value.trim();
      const amount  = document.getElementById('amount').value.trim();
      const status  = document.getElementById('status');
      const btn     = document.getElementById('sendBtn');

      if (!address || !amount) {
        setStatus('Please fill in all fields', 'err');
        return;
      }

      btn.disabled = true;
      setStatus('Sending…', '');

      MDS.cmd(\`send address:\${address} amount:\${amount}\`, res => {
        btn.disabled = false;
        if (res.status) {
          setStatus('✓ Sent successfully', 'ok');
          loadBalance();
        } else {
          setStatus('✗ ' + (res.error || 'Unknown error'), 'err');
        }
      });
    }

    function setStatus(msg, cls) {
      const el = document.getElementById('status');
      el.textContent = msg;
      el.className = 'status ' + cls;
    }
  </script>
</body>
</html>`,
        },
        {
            path: 'mds.js',
            content: `/**
 * MDS.js — Minima MiniDapp Service library stub
 * Replace with the real mds.js from your Minima node.
 * Real file location: http://localhost:9003/mds.js
 *
 * This stub is for development/testing only.
 */

var MDS = (function() {
  var _callback = null;
  var _initialized = false;

  function init(callback) {
    _callback = callback;
    // In real Minima environment, this connects to the node.
    // Stub: simulate inited after 100ms
    setTimeout(function() {
      _initialized = true;
      if (_callback) _callback({ event: 'inited', data: {} });
    }, 100);
  }

  function cmd(command, callback) {
    console.log('[MDS stub] cmd:', command);
    // Stub responses — replace with real MDS
    if (command === 'balance') {
      callback({ status: true, response: [{ token: { name: 'Minima', tokenid: '0x00' }, confirmed: '100', sendable: '99.99' }] });
    } else {
      callback({ status: true, response: {} });
    }
  }

  function log(msg) { console.log('[MDS]', msg); }

  return { init, cmd, log };
})();
`,
        },
        {
            path: 'README.md',
            content: `# {{NAME}}

A MiniDapp built with [create-minidapp](https://github.com/minima-global/minima-developer-toolkit).

## Development

Install the MiniDapp on your Minima node:

1. Run \`npm run package\` to create the zip
2. Open your Minima node at \`http://localhost:9003\`
3. Install the \`.mds.zip\` file

## Structure

\`\`\`
{{NAME}}/
├── dapp.conf       # MiniDapp manifest
├── index.html      # Main UI
├── mds.js          # Minima MDS library (replace with real one from node)
└── README.md
\`\`\`

## Notes

- Replace \`mds.js\` with the real file from your node: \`http://localhost:9003/mds.js\`
- MiniDapps run entirely in your Minima node — no central servers
- Test locally at \`http://localhost:9003/{{NAME_LOWER}}/index.html\`
`,
        },
        {
            path: 'package.json',
            content: `{
  "name": "{{NAME_LOWER}}",
  "version": "1.0.0",
  "description": "{{DESCRIPTION}}",
  "scripts": {
    "package": "zip -r {{NAME_LOWER}}.mds.zip . -x '*.zip' -x 'node_modules/*' -x '.git/*' -x 'package.json' -x 'package-lock.json'"
  }
}
`,
        },
    ],
};
// ─── COUNTER TEMPLATE ──────────────────────────────────────────────────────────
const counterTemplate = {
    name: 'counter',
    description: 'Stateful counter using KISS VM state variables',
    files: [
        {
            path: 'dapp.conf',
            content: `{
  "name": "{{NAME}}",
  "icon": "icon.png",
  "version": "1.0.0",
  "description": "{{DESCRIPTION}}",
  "browser": "index.html"
}`,
        },
        {
            path: 'index.html',
            content: `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>{{NAME}}</title>
  <script src="mds.js"></script>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: system-ui, sans-serif; background: #0f0f0f; color: #f0f0f0; padding: 24px; display: flex; flex-direction: column; align-items: center; min-height: 100vh; justify-content: center; }
    h1  { font-size: 1.5rem; margin-bottom: 32px; color: #00d4aa; }
    .counter { font-size: 5rem; font-weight: 700; margin-bottom: 32px; }
    .btns { display: flex; gap: 16px; }
    button { background: #00d4aa; color: #000; border: none; border-radius: 8px; padding: 16px 32px; font-size: 1.25rem; font-weight: 700; cursor: pointer; }
    button.dec { background: #ff6b6b; }
    .status { margin-top: 24px; font-size: 0.875rem; color: #888; }
    .coins { margin-top: 24px; font-size: 0.75rem; color: #555; max-width: 500px; word-break: break-all; text-align: center; }
  </style>
</head>
<body>
  <h1>{{NAME}} Counter</h1>
  <div class="counter" id="count">0</div>
  <div class="btns">
    <button class="dec" onclick="decrement()">−</button>
    <button onclick="increment()">+</button>
  </div>
  <div class="status" id="status">Initializing…</div>
  <div class="coins" id="coins"></div>

  <script>
    // Counter contract script (KISS VM)
    // STATE(1) holds current count as a number
    // Spends existing coin, creates new coin with count+1 / count-1
    const CONTRACT = \`
      LET count = STATE(1)
      LET newcount = PREVSTATE(1)
      RETURN SIGNEDBY(@ADDRESS) AND newcount EQ count + 1 OR newcount EQ count - 1
    \`.trim();

    var counterCoin = null;

    MDS.init(msg => {
      if (msg.event === 'inited') {
        setStatus('Connected to node');
        findCounterCoin();
      }
    });

    function findCounterCoin() {
      MDS.cmd('coins address:@ADDRESS relevant:true', res => {
        if (!res.status) { setStatus('Error loading coins'); return; }
        const coins = res.response || [];
        counterCoin = coins.find(c => c.state && c.state['1'] !== undefined) || null;
        const count = counterCoin ? parseInt(counterCoin.state['1']) : 0;
        document.getElementById('count').textContent = count;
        setStatus(counterCoin ? 'Counter coin found' : 'No counter coin — create one first');
      });
    }

    function increment() { updateCount(1); }
    function decrement() { updateCount(-1); }

    function updateCount(delta) {
      if (!counterCoin) { setStatus('No counter coin found', true); return; }
      const current = parseInt(counterCoin.state['1']);
      const next = current + delta;
      setStatus('Building transaction…');
      // In a real MiniDapp you would use txncreate/txninput/txnoutput with PREVSTATE
      // This stub just shows the intent
      MDS.log('Counter update: ' + current + ' -> ' + next);
      setStatus('Counter: ' + current + ' → ' + next + ' (tx pending)');
    }

    function setStatus(msg, err) {
      const el = document.getElementById('status');
      el.textContent = msg;
      el.style.color = err ? '#ff6b6b' : '#888';
    }
  </script>
</body>
</html>`,
        },
        {
            path: 'mds.js',
            content: `/**
 * MDS.js stub — replace with real mds.js from your Minima node
 * http://localhost:9003/mds.js
 */
var MDS = (function() {
  function init(cb) { setTimeout(() => cb({ event: 'inited' }), 100); }
  function cmd(c, cb) { console.log('[MDS stub]', c); cb({ status: true, response: [] }); }
  function log(m) { console.log('[MDS]', m); }
  return { init, cmd, log };
})();`,
        },
        {
            path: 'contract.kiss',
            content: `/*
 * Counter contract — KISS VM
 *
 * STATE(1) = current count
 * PREVSTATE(1) = new count (must be current ± 1)
 *
 * Allows increment and decrement only.
 * Only the owner can update.
 */

LET count    = STATE(1)
LET newcount = PREVSTATE(1)

ASSERT SIGNEDBY(@ADDRESS)

RETURN newcount EQ count + 1 OR newcount EQ count - 1
`,
        },
        {
            path: 'README.md',
            content: `# {{NAME}} — Counter MiniDapp

Demonstrates on-chain state using KISS VM STATE variables.

## How it works

Each increment/decrement spends the counter coin and creates a new one with the updated count stored in \`STATE(1)\`.

The contract (\`contract.kiss\`) enforces:
- Only the owner can update (\`SIGNEDBY\`)
- Count can only change by exactly ±1 (prevents jumping)

## Contract

\`\`\`
LET count    = STATE(1)
LET newcount = PREVSTATE(1)
ASSERT SIGNEDBY(@ADDRESS)
RETURN newcount EQ count + 1 OR newcount EQ count - 1
\`\`\`

## Testing the contract

\`\`\`bash
npx minima-test eval "LET count = STATE(1) LET newcount = PREVSTATE(1) ASSERT SIGNEDBY(@ADDRESS) RETURN newcount EQ count + 1 OR newcount EQ count - 1"
\`\`\`
`,
        },
        {
            path: 'package.json',
            content: `{
  "name": "{{NAME_LOWER}}",
  "version": "1.0.0",
  "scripts": {
    "package": "zip -r {{NAME_LOWER}}.mds.zip . -x '*.zip' -x 'node_modules/*'"
  }
}`,
        },
    ],
};
// ─── EXCHANGE TEMPLATE ─────────────────────────────────────────────────────────
const exchangeTemplate = {
    name: 'exchange',
    description: 'Atomic token swap using KISS VM VERIFYOUT',
    files: [
        {
            path: 'dapp.conf',
            content: `{
  "name": "{{NAME}}",
  "icon": "icon.png",
  "version": "1.0.0",
  "description": "{{DESCRIPTION}}",
  "browser": "index.html"
}`,
        },
        {
            path: 'index.html',
            content: `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>{{NAME}}</title>
  <script src="mds.js"></script>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: system-ui, sans-serif; background: #0f0f0f; color: #f0f0f0; padding: 24px; }
    h1  { font-size: 1.5rem; margin-bottom: 8px; color: #00d4aa; }
    p   { color: #888; margin-bottom: 24px; font-size: 0.875rem; }
    .card { background: #1a1a1a; border-radius: 12px; padding: 20px; margin-bottom: 16px; }
    .label { font-size: 0.75rem; color: #888; text-transform: uppercase; letter-spacing: 0.05em; margin-bottom: 8px; }
    input  { width: 100%; background: #111; border: 1px solid #333; border-radius: 8px; padding: 10px 14px; color: #f0f0f0; font-size: 1rem; margin-bottom: 10px; outline: none; }
    input:focus { border-color: #00d4aa; }
    button { background: #00d4aa; color: #000; border: none; border-radius: 8px; padding: 12px 24px; font-size: 1rem; font-weight: 600; cursor: pointer; width: 100%; }
    .arrow { text-align: center; font-size: 1.5rem; color: #444; margin: 8px 0; }
    .status { margin-top: 12px; font-size: 0.875rem; color: #888; }
    .status.ok  { color: #00d4aa; }
    .status.err { color: #ff6b6b; }
    code { background: #111; padding: 12px; display: block; border-radius: 8px; font-size: 0.75rem; color: #00d4aa; word-break: break-all; margin-top: 8px; }
  </style>
</head>
<body>
  <h1>{{NAME}} — Atomic Swap</h1>
  <p>Trustless token exchange using KISS VM VERIFYOUT</p>

  <div class="card">
    <div class="label">You Send</div>
    <input id="sendAmount" placeholder="Amount" type="number" />
    <input id="sendToken" placeholder="Token ID (0x00 = Minima)" />
  </div>
  <div class="arrow">↕</div>
  <div class="card">
    <div class="label">You Receive</div>
    <input id="recvAmount" placeholder="Amount" type="number" />
    <input id="recvToken" placeholder="Token ID" />
    <input id="recvAddress" placeholder="Your receive address (0x…)" />
  </div>

  <div class="card">
    <div class="label">Counterparty address</div>
    <input id="counterparty" placeholder="Their address (0x…)" />
    <button onclick="createSwapOffer()">Create Swap Offer</button>
    <div class="status" id="status"></div>
  </div>

  <div class="card" id="contractCard" style="display:none">
    <div class="label">Generated Contract Script</div>
    <code id="contractScript"></code>
  </div>

  <script>
    MDS.init(msg => {
      if (msg.event === 'inited') {
        document.getElementById('status').textContent = 'Ready';
      }
    });

    function createSwapOffer() {
      const sendAmount   = document.getElementById('sendAmount').value;
      const sendToken    = document.getElementById('sendToken').value || '0x00';
      const recvAmount   = document.getElementById('recvAmount').value;
      const recvToken    = document.getElementById('recvToken').value;
      const recvAddress  = document.getElementById('recvAddress').value;
      const counterparty = document.getElementById('counterparty').value;

      if (!sendAmount || !recvAmount || !recvToken || !recvAddress || !counterparty) {
        setStatus('Please fill all fields', 'err');
        return;
      }

      // Build exchange contract
      const script = \`RETURN VERIFYOUT(@INPUT \${recvAmount} \${recvAddress} \${recvToken})\`;
      document.getElementById('contractScript').textContent = script;
      document.getElementById('contractCard').style.display = 'block';

      setStatus('Contract ready — send coin to this script address', 'ok');
      MDS.log('Swap contract: ' + script);
    }

    function setStatus(msg, cls) {
      const el = document.getElementById('status');
      el.textContent = msg;
      el.className = 'status ' + (cls || '');
    }
  </script>
</body>
</html>`,
        },
        {
            path: 'mds.js',
            content: `/**
 * MDS.js stub — replace with real mds.js from your Minima node
 */
var MDS = (function() {
  function init(cb) { setTimeout(() => cb({ event: 'inited' }), 100); }
  function cmd(c, cb) { console.log('[MDS stub]', c); cb({ status: true, response: {} }); }
  function log(m) { console.log('[MDS]', m); }
  return { init, cmd, log };
})();`,
        },
        {
            path: 'contract.kiss',
            content: `/*
 * Exchange contract — KISS VM
 *
 * Atomic swap: locks coin until counterparty pays the agreed amount
 * to the agreed address.
 *
 * Parameters (replace with actual values):
 *   RECV_AMOUNT  = amount to receive
 *   RECV_ADDRESS = address to receive payment at
 *   RECV_TOKEN   = token ID to receive
 *
 * Usage: RETURN VERIFYOUT(@INPUT RECV_AMOUNT RECV_ADDRESS RECV_TOKEN)
 */

RETURN VERIFYOUT(@INPUT {{RECV_AMOUNT}} {{RECV_ADDRESS}} {{RECV_TOKEN}})
`,
        },
        {
            path: 'README.md',
            content: `# {{NAME}} — Atomic Swap MiniDapp

Demonstrates trustless token exchange using KISS VM \`VERIFYOUT\`.

## How it works

The exchange contract locks your coin until the counterparty creates a transaction that:
1. Spends your locked coin
2. Outputs the agreed token/amount to your address

\`VERIFYOUT\` enforces this atomically — either both sides execute or neither does.

## Contract

\`\`\`
RETURN VERIFYOUT(@INPUT <amount> <yourAddress> <tokenId>)
\`\`\`

- \`@INPUT\` = index of the input being spent
- \`amount\` = what you want to receive
- \`yourAddress\` = where to receive it
- \`tokenId\` = which token (0x00 = Minima)

## Testing

\`\`\`bash
npx minima-test eval "RETURN VERIFYOUT(@INPUT 100 0xYOURADDRESS 0x00)"
\`\`\`
`,
        },
        {
            path: 'package.json',
            content: `{
  "name": "{{NAME_LOWER}}",
  "version": "1.0.0",
  "scripts": {
    "package": "zip -r {{NAME_LOWER}}.mds.zip . -x '*.zip' -x 'node_modules/*'"
  }
}`,
        },
    ],
};
exports.TEMPLATES = {
    basic: basicTemplate,
    counter: counterTemplate,
    exchange: exchangeTemplate,
};
function listTemplates() {
    console.log('\nAvailable templates:\n');
    for (const [key, tpl] of Object.entries(exports.TEMPLATES)) {
        console.log(`  ${key.padEnd(12)} ${tpl.description}`);
    }
    console.log('');
}
//# sourceMappingURL=templates.js.map