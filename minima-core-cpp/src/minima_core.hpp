#pragma once
/**
 * minima_core.hpp — single-header convenience include.
 *
 * Include this file to get the full Minima core API.
 * For fine-grained includes, use individual headers directly.
 */

// Types
#include "types/MiniNumber.hpp"
#include "types/MiniData.hpp"
#include "types/MiniString.hpp"

// Protocol objects
#include "objects/Address.hpp"
#include "objects/StateVariable.hpp"
#include "objects/Coin.hpp"
#include "objects/Transaction.hpp"
#include "objects/Witness.hpp"
#include "objects/TxHeader.hpp"
#include "objects/TxBody.hpp"
#include "objects/TxPoW.hpp"

// KISS VM
#include "kissvm/Value.hpp"
#include "kissvm/Token.hpp"
#include "kissvm/Tokenizer.hpp"
#include "kissvm/Environment.hpp"
#include "kissvm/Contract.hpp"
#include "kissvm/functions/Functions.hpp"

// Crypto
#include "crypto/Hash.hpp"
#include "crypto/Schnorr.hpp"

// Serialisation
#include "serialization/DataStream.hpp"

// Validation
#include "validation/TxPoWValidator.hpp"
