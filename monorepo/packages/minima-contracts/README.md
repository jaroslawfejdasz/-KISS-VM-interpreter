# minima-contracts

Production-ready KISS VM smart contract templates for the [Minima](https://minima.global) blockchain.

## Install

```bash
npm install minima-contracts
```

## Contracts

| Contract | Description |
|----------|-------------|
| `timelockContract` | Funds locked until a specific block height |
| `multisigContract` | M-of-N multi-signature |
| `htlcContract` | Hash Time Locked Contract (atomic swaps) |
| `paymentChannelContract` | Two-party payment channel |
| `nftContract` | NFT ownership transfer guard |
| `escrowContract` | 2-of-3 escrow with arbiter |
| `vaultContract` | Daily spending limit (hot + cold wallet) |

## Usage

```js
import {
  timelockContract,
  multisigContract,
  htlcContract,
  escrowContract,
  vaultContract,
  getTemplate,
} from 'minima-contracts';

// Timelock — unlock at block 100000, owner signs
const timelock = timelockContract(100000n, '0xYOUR_ADDRESS');

// 2-of-3 multisig
const multisig = multisigContract(2, ['0xALICE', '0xBOB', '0xCHARLIE']);

// HTLC (atomic swap)
const htlc = htlcContract(
  '0xSHA3_HASH_OF_SECRET',  // hashlock
  '0xRECIPIENT',             // claim with preimage
  '0xREFUND',                // refund after timeout
  50000n,                    // timeout block
);

// Escrow with arbiter
const escrow = escrowContract('0xBUYER', '0xSELLER', '0xARBITER');

// Vault: hot wallet limited to 100 MINIMA per tx, cold wallet unlimited
const vault = vaultContract('0xHOT', '0xCOLD', 100n);
```

### Using template registry

```js
import { getTemplate } from 'minima-contracts';

const script = getTemplate('timelock', {
  lockBlock: '100000',
  ownerAddress: '0xYOUR_ADDRESS',
});
```

## Security notes

All contracts are designed to:
- Prevent unauthorized access (require SIGNEDBY or MULTISIG)
- Prevent replay attacks (use SAMESTATE or state variable checks where applicable)
- Provide fallback `RETURN FALSE` to deny by default
- Follow Minima's UTxO model (each coin has an independent script)

Use [kiss-vm-lint](https://www.npmjs.com/package/kiss-vm-lint) to statically analyze generated scripts before deployment.

## Resources

- [KISS VM Reference](https://docs.minima.global/learn/smart-contracts/kiss-vm)
- [Minima Docs](https://docs.minima.global)

## License

MIT
