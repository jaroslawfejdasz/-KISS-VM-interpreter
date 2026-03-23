# create-minidapp

Scaffold a new [Minima](https://minima.global) MiniDapp project in seconds.

## Usage

```bash
npx @jaroslawfejdasz/create-minidapp my-app
```

```bash
npx @jaroslawfejdasz/create-minidapp my-wallet \
  --description "My Minima wallet" \
  --author "Your Name" \
  --category DeFi \
  --with-contract \
  --with-maxima
```

## Options

| Option | Default | Description |
|--------|---------|-------------|
| `--description <text>` | `<name> MiniDapp` | App description |
| `--author <name>` | — | Author name |
| `--category <cat>` | `Utility` | `Business` \| `Utility` \| `DeFi` \| `NFT` \| `Social` |
| `--with-contract` | false | Include KISS VM contract scaffold |
| `--with-maxima` | false | Include Maxima messaging service worker |

## What's generated

```
my-app/
├── dapp.conf           # MiniDapp metadata (name, version, icon, permissions)
├── icon.png            # Default 1×1 icon — replace with your own (96×96 recommended)
├── index.html          # Main UI with dark Minima-themed layout
├── app.js              # MDS API integration (balance, address)
├── mds.js              # Dev stub — replace with real mds.js from your node
├── service.js          # (--with-maxima) Background Maxima message handler
├── contracts/
│   └── main.kiss       # (--with-contract) KISS VM contract scaffold
├── .gitignore
└── README.md
```

## Deploying

```bash
# Package as ZIP
cd my-app
zip -r my-app.zip . -x "*.git*"

# Install on your Minima node
# Open http://localhost:9003 → Dapp Manager → Install → Upload my-app.zip
```

For production, replace `mds.js` with the real one from your node:
```
http://localhost:9003/mds.js
```

## Resources

- [Minima MiniDapp Docs](https://docs.minima.global/buildondeminima/minidapps)
- [MDS API Reference](https://docs.minima.global/buildondeminima/minidapps/mds)
- [KISS VM Reference](https://docs.minima.global/learn/smart-contracts/kiss-vm)

## License

MIT
