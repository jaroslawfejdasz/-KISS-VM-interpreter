#include "MiniData.hpp"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

// SHA3-256 — Keccak reference implementation (no external deps)
// Based on the NIST Keccak reference, condensed for clarity
namespace keccak {

static const uint64_t RC[24] = {
    0x0000000000000001ULL,0x0000000000008082ULL,0x800000000000808aULL,
    0x8000000080008000ULL,0x000000000000808bULL,0x0000000080000001ULL,
    0x8000000080008081ULL,0x8000000000008009ULL,0x000000000000008aULL,
    0x0000000000000088ULL,0x0000000080008009ULL,0x000000008000000aULL,
    0x000000008000808bULL,0x800000000000008bULL,0x8000000000008089ULL,
    0x8000000000008003ULL,0x8000000000008002ULL,0x8000000000000080ULL,
    0x000000000000800aULL,0x800000008000000aULL,0x8000000080008081ULL,
    0x8000000000008080ULL,0x0000000080000001ULL,0x8000000080008008ULL
};
static const int RHO[24] = {1,3,6,10,15,21,28,36,45,55,2,14,27,41,56,8,25,43,62,18,39,61,20,44};
static const int PI[24]  = {10,7,11,17,18,3,5,16,8,21,24,4,15,23,19,13,12,2,20,14,22,9,6,1};

static uint64_t rotl64(uint64_t v, int n) { return (v << n) | (v >> (64-n)); }

static void keccakf(uint64_t s[25]) {
    for (int r = 0; r < 24; ++r) {
        uint64_t C[5];
        for (int i=0;i<5;++i) C[i]=s[i]^s[i+5]^s[i+10]^s[i+15]^s[i+20];
        for (int i=0;i<5;++i) { uint64_t d=C[(i+4)%5]^rotl64(C[(i+1)%5],1); for(int j=0;j<25;j+=5) s[j+i]^=d; }
        uint64_t t=s[1];
        for (int i=0;i<24;++i) { int j=PI[i]; uint64_t tmp=s[j]; s[j]=rotl64(t,RHO[i]); t=tmp; }
        for (int j=0;j<25;j+=5) {
            uint64_t t0=s[j],t1=s[j+1],t2=s[j+2],t3=s[j+3],t4=s[j+4];
            s[j]^=(~t1)&t2; s[j+1]^=(~t2)&t3; s[j+2]^=(~t3)&t4; s[j+3]^=(~t4)&t0; s[j+4]^=(~t0)&t1;
        }
        s[0]^=RC[r];
    }
}

// SHA3-256 (not Keccak-256 — uses NIST padding 0x06)
static std::vector<uint8_t> sha3_256(const uint8_t* msg, size_t len) {
    const size_t rate = 136; // (1600 - 256*2) / 8
    uint64_t s[25] = {};
    std::vector<uint8_t> padded(msg, msg+len);
    padded.push_back(0x06);
    while (padded.size() % rate != 0) padded.push_back(0x00);
    padded[padded.size()-1] |= 0x80;

    for (size_t i = 0; i < padded.size(); i += rate) {
        for (size_t j = 0; j < rate/8; ++j) {
            uint64_t word = 0;
            memcpy(&word, &padded[i + j*8], 8);
            // little-endian already on x86; for portability use byte loop
            s[j] ^= word;
        }
        keccakf(s);
    }
    std::vector<uint8_t> digest(32);
    memcpy(digest.data(), s, 32);
    return digest;
}

// SHA2-256
struct SHA2State {
    uint32_t h[8];
    uint64_t count{0};
    uint8_t  buf[64]{};
    uint8_t  bufLen{0};
};

static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t rotr32(uint32_t v,int n){return(v>>n)|(v<<(32-n));}
static void sha256_block(uint32_t h[8], const uint8_t blk[64]) {
    uint32_t w[64];
    for(int i=0;i<16;++i) w[i]=(blk[i*4]<<24)|(blk[i*4+1]<<16)|(blk[i*4+2]<<8)|blk[i*4+3];
    for(int i=16;i<64;++i){
        uint32_t s0=rotr32(w[i-15],7)^rotr32(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1=rotr32(w[i-2],17)^rotr32(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
    for(int i=0;i<64;++i){
        uint32_t S1=rotr32(e,6)^rotr32(e,11)^rotr32(e,25);
        uint32_t ch=(e&f)^((~e)&g);
        uint32_t t1=hh+S1+ch+K256[i]+w[i];
        uint32_t S0=rotr32(a,2)^rotr32(a,13)^rotr32(a,22);
        uint32_t maj=(a&b)^(a&c)^(b&c);
        uint32_t t2=S0+maj;
        hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
}

static std::vector<uint8_t> sha2_256(const uint8_t* msg, size_t len) {
    uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    // Padding
    std::vector<uint8_t> padded(msg, msg+len);
    padded.push_back(0x80);
    while((padded.size()%64)!=56) padded.push_back(0);
    uint64_t bits=(uint64_t)len*8;
    for(int i=7;i>=0;--i) padded.push_back((bits>>(i*8))&0xff);
    for(size_t i=0;i<padded.size();i+=64) sha256_block(h, padded.data()+i);
    std::vector<uint8_t> out(32);
    for(int i=0;i<8;++i){out[i*4]=(h[i]>>24)&0xff;out[i*4+1]=(h[i]>>16)&0xff;out[i*4+2]=(h[i]>>8)&0xff;out[i*4+3]=h[i]&0xff;}
    return out;
}

} // namespace keccak

namespace minima {

// ─── hex helpers ──────────────────────────────────────────────────────────────

static uint8_t hexCharVal(char c) {
    if (c>='0'&&c<='9') return c-'0';
    if (c>='a'&&c<='f') return c-'a'+10;
    if (c>='A'&&c<='F') return c-'A'+10;
    throw std::invalid_argument(std::string("MiniData: invalid hex char: ") + c);
}

static std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::string h = hex;
    if (h.size() >= 2 && h[0]=='0' && (h[1]=='x'||h[1]=='X')) h = h.substr(2);
    if (h.empty()) throw std::invalid_argument("MiniData: empty hex literal");
    if (h.size() % 2 != 0) h = "0" + h; // pad
    std::vector<uint8_t> out(h.size()/2);
    for (size_t i=0;i<out.size();++i)
        out[i] = (hexCharVal(h[i*2]) << 4) | hexCharVal(h[i*2+1]);
    return out;
}

// ─── construction ─────────────────────────────────────────────────────────────

MiniData::MiniData(const std::vector<uint8_t>& bytes) : m_bytes(bytes) {}
MiniData::MiniData(const std::string& hexStr) : m_bytes(hexToBytes(hexStr)) {}

MiniData MiniData::fromHex(const std::string& hex)              { return MiniData(hexToBytes(hex)); }
MiniData MiniData::fromBytes(const uint8_t* data, size_t len)   { return MiniData(std::vector<uint8_t>(data, data+len)); }
MiniData MiniData::zeroes(size_t len)                           { return MiniData(std::vector<uint8_t>(len, 0)); }

// ─── operations ───────────────────────────────────────────────────────────────

MiniData MiniData::concat(const MiniData& other) const {
    std::vector<uint8_t> out = m_bytes;
    out.insert(out.end(), other.m_bytes.begin(), other.m_bytes.end());
    return MiniData(out);
}

MiniData MiniData::subset(size_t start, size_t len) const {
    if (start + len > m_bytes.size())
        throw std::out_of_range("MiniData::subset out of range");
    return MiniData(std::vector<uint8_t>(m_bytes.begin()+start, m_bytes.begin()+start+len));
}

MiniData MiniData::rev() const {
    std::vector<uint8_t> out(m_bytes.rbegin(), m_bytes.rend());
    return MiniData(out);
}

MiniData MiniData::sha2() const {
    return MiniData(keccak::sha2_256(m_bytes.data(), m_bytes.size()));
}

MiniData MiniData::sha3() const {
    return MiniData(keccak::sha3_256(m_bytes.data(), m_bytes.size()));
}

// ─── conversions ──────────────────────────────────────────────────────────────

std::string MiniData::toHexString(bool prefix0x) const {
    std::ostringstream oss;
    if (prefix0x) oss << "0x";
    for (uint8_t b : m_bytes)
        oss << std::hex << std::setw(2) << std::setfill('0') << std::uppercase << (int)b;
    return oss.str();
}

// ─── serialisation ────────────────────────────────────────────────────────────

std::vector<uint8_t> MiniData::serialise() const {
    // 4-byte big-endian length + raw bytes
    uint32_t len = (uint32_t)m_bytes.size();
    std::vector<uint8_t> out;
    out.push_back((len >> 24) & 0xff);
    out.push_back((len >> 16) & 0xff);
    out.push_back((len >>  8) & 0xff);
    out.push_back( len        & 0xff);
    out.insert(out.end(), m_bytes.begin(), m_bytes.end());
    return out;
}

MiniData MiniData::deserialise(const uint8_t* data, size_t& offset) {
    uint32_t len = ((uint32_t)data[offset]<<24)|((uint32_t)data[offset+1]<<16)|
                   ((uint32_t)data[offset+2]<<8)|(uint32_t)data[offset+3];
    offset += 4;
    MiniData md = MiniData::fromBytes(data + offset, len);
    offset += len;
    return md;
}

} // namespace minima
