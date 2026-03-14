#include "MiniData.hpp"
#include <stdexcept>
#include <cctype>
#include <cstring>
#include <random>

// SHA3-256 and SHA2-256 implementations (no external deps)
namespace keccak_impl {
static const uint64_t RC[24]={
0x0000000000000001ULL,0x0000000000008082ULL,0x800000000000808aULL,0x8000000080008000ULL,
0x000000000000808bULL,0x0000000080000001ULL,0x8000000080008081ULL,0x8000000000008009ULL,
0x000000000000008aULL,0x0000000000000088ULL,0x0000000080008009ULL,0x000000008000000aULL,
0x000000008000808bULL,0x800000000000008bULL,0x8000000000008089ULL,0x8000000000008003ULL,
0x8000000000008002ULL,0x8000000000000080ULL,0x000000000000800aULL,0x800000008000000aULL,
0x8000000080008081ULL,0x8000000000008080ULL,0x0000000080000001ULL,0x8000000080008008ULL};
static const int ROT[24]={1,3,6,10,15,21,28,36,45,55,2,14,27,41,56,8,25,43,62,18,39,61,20,44};
static const int PI[24]={10,7,11,17,18,3,5,16,8,21,24,4,15,23,19,13,12,2,20,14,22,9,6,1};
static void keccakf(uint64_t s[25]) {
    for(int r=0;r<24;++r){
        uint64_t bc[5];
        for(int i=0;i<5;++i) bc[i]=s[i]^s[i+5]^s[i+10]^s[i+15]^s[i+20];
        for(int i=0;i<5;++i){ uint64_t t=bc[(i+4)%5]^((bc[(i+1)%5]<<1)|(bc[(i+1)%5]>>63)); for(int j=0;j<25;j+=5) s[j+i]^=t; }
        uint64_t last=s[1];
        for(int i=0;i<24;++i){ int j=PI[i]; uint64_t t=s[j]; int ro=ROT[i]; s[j]=(last<<ro)|(last>>(64-ro)); last=t; }
        for(int j=0;j<25;j+=5){ uint64_t t[5]; for(int i=0;i<5;++i) t[i]=s[j+i]; for(int i=0;i<5;++i) s[j+i]^=(~t[(i+1)%5])&t[(i+2)%5]; }
        s[0]^=RC[r];
    }
}
static std::vector<uint8_t> sha3_256(const uint8_t* msg, size_t len) {
    static const int RATE=136;
    uint64_t s[25]={};
    size_t i=0;
    while(i+RATE<=len){
        for(int b=0;b<RATE;b+=8){ uint64_t v=0; for(int k=0;k<8;++k) v|=((uint64_t)msg[i+b+k])<<(8*k); s[b/8]^=v; }
        keccakf(s); i+=RATE;
    }
    uint8_t pad[RATE]={};
    memcpy(pad,msg+i,len-i);
    pad[len-i]=0x06; pad[RATE-1]^=0x80;
    for(int b=0;b<RATE;b+=8){ uint64_t v=0; for(int k=0;k<8;++k) v|=((uint64_t)pad[b+k])<<(8*k); s[b/8]^=v; }
    keccakf(s);
    std::vector<uint8_t> out(32);
    for(int b=0;b<32;++b) out[b]=(s[b/8]>>(8*(b%8)))&0xFF;
    return out;
}
static std::vector<uint8_t> sha2_256(const uint8_t* msg, size_t len) {
    static const uint32_t K[64]={
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    auto rr=[](uint32_t v,int n){return(v>>n)|(v<<(32-n));};
    size_t ol=len;
    std::vector<uint8_t> data(msg,msg+len);
    data.push_back(0x80);
    while(data.size()%64!=56) data.push_back(0);
    uint64_t bits=(uint64_t)ol*8;
    for(int i=7;i>=0;--i) data.push_back((bits>>(i*8))&0xFF);
    for(size_t i=0;i<data.size();i+=64){
        uint32_t w[64];
        for(int j=0;j<16;++j) w[j]=((uint32_t)data[i+j*4]<<24)|((uint32_t)data[i+j*4+1]<<16)|((uint32_t)data[i+j*4+2]<<8)|(uint32_t)data[i+j*4+3];
        for(int j=16;j<64;++j){ uint32_t s0=rr(w[j-15],7)^rr(w[j-15],18)^(w[j-15]>>3); uint32_t s1=rr(w[j-2],17)^rr(w[j-2],19)^(w[j-2]>>10); w[j]=w[j-16]+s0+w[j-7]+s1; }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for(int j=0;j<64;++j){ uint32_t S1=rr(e,6)^rr(e,11)^rr(e,25); uint32_t ch=(e&f)^(~e&g); uint32_t t1=hh+S1+ch+K[j]+w[j]; uint32_t S0=rr(a,2)^rr(a,13)^rr(a,22); uint32_t maj=(a&b)^(a&c)^(b&c); uint32_t t2=S0+maj; hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2; }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
    }
    std::vector<uint8_t> out(32);
    for(int i=0;i<8;++i){ out[i*4]=(h[i]>>24)&0xFF;out[i*4+1]=(h[i]>>16)&0xFF;out[i*4+2]=(h[i]>>8)&0xFF;out[i*4+3]=h[i]&0xFF; }
    return out;
}
} // namespace keccak_impl

namespace minima {

const MiniData MiniData::ZERO_TXPOWID = MiniData::fromHex("0x00");
const MiniData MiniData::ONE_TXPOWID  = MiniData::fromHex("0x01");

static uint8_t hexNibble(char c) {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return c-'a'+10;
    if(c>='A'&&c<='F') return c-'A'+10;
    throw std::invalid_argument(std::string("Invalid hex: ")+c);
}

MiniData MiniData::fromHex(const std::string& hex) {
    std::string h=hex;
    if(h.size()>=2&&h[0]=='0'&&(h[1]=='x'||h[1]=='X')) h=h.substr(2);
    for(auto& c:h) c=(char)toupper((unsigned char)c);
    if(h.empty()) return MiniData();
    if(h.size()%2!=0) h="0"+h;
    std::vector<uint8_t> out(h.size()/2);
    for(size_t i=0;i<out.size();++i) out[i]=(hexNibble(h[i*2])<<4)|hexNibble(h[i*2+1]);
    return MiniData(out);
}

MiniData MiniData::withMinLength(const std::vector<uint8_t>& data, int minLen) {
    if((int)data.size()>=minLen) return MiniData(data);
    std::vector<uint8_t> out(minLen,0);
    int diff=minLen-(int)data.size();
    std::copy(data.begin(),data.end(),out.begin()+diff);
    return MiniData(out);
}

MiniData MiniData::getRandomData(int len) {
    static std::random_device rd;
    static std::mt19937_64 rng(rd());
    std::vector<uint8_t> out(len);
    for(auto& b:out) b=(uint8_t)(rng()&0xFF);
    return MiniData(out);
}

bool MiniData::isLess(const MiniData& o) const {
    if(m_bytes.size()!=o.m_bytes.size()) return m_bytes.size()<o.m_bytes.size();
    return m_bytes<o.m_bytes;
}

std::string MiniData::to0xString() const { return toHexString(true); }
std::string MiniData::toHexString(bool prefix) const {
    static const char* HEX="0123456789ABCDEF";
    std::string out; if(prefix) out="0x";
    for(uint8_t b:m_bytes){ out+=HEX[b>>4]; out+=HEX[b&0xF]; }
    return out;
}

MiniData MiniData::sha3() const {
    return MiniData(keccak_impl::sha3_256(m_bytes.data(),m_bytes.size()));
}
MiniData MiniData::sha2() const {
    return MiniData(keccak_impl::sha2_256(m_bytes.data(),m_bytes.size()));
}

std::vector<uint8_t> MiniData::serialise() const {
    uint32_t len=(uint32_t)m_bytes.size();
    std::vector<uint8_t> out;
    out.push_back((len>>24)&0xFF); out.push_back((len>>16)&0xFF);
    out.push_back((len>> 8)&0xFF); out.push_back( len     &0xFF);
    out.insert(out.end(),m_bytes.begin(),m_bytes.end());
    return out;
}
MiniData MiniData::deserialise(const uint8_t* data, size_t& offset) {
    uint32_t len=((uint32_t)data[offset+0]<<24)|((uint32_t)data[offset+1]<<16)|
                 ((uint32_t)data[offset+2]<< 8)|((uint32_t)data[offset+3]);
    offset+=4;
    if((int)len>MINIMA_MAX_MINIDATA_LENGTH) throw std::runtime_error("MiniData: too large");
    MiniData r(std::vector<uint8_t>(data+offset,data+offset+len));
    offset+=len;
    return r;
}
MiniData MiniData::deserialise(const uint8_t* data, size_t& offset, int expectedSize) {
    MiniData r=deserialise(data,offset);
    if((int)r.size()!=expectedSize)
        throw std::runtime_error("MiniData: expected "+std::to_string(expectedSize)+" got "+std::to_string(r.size()));
    return r;
}
std::vector<uint8_t> MiniData::serialiseHash() const {
    if((int)m_bytes.size()>MINIMA_MAX_HASH_LENGTH)
        throw std::runtime_error("MiniData: hash>64: "+std::to_string(m_bytes.size()));
    return serialise();
}
MiniData MiniData::deserialiseHash(const uint8_t* data, size_t& offset) {
    uint32_t len=((uint32_t)data[offset+0]<<24)|((uint32_t)data[offset+1]<<16)|
                 ((uint32_t)data[offset+2]<< 8)|((uint32_t)data[offset+3]);
    offset+=4;
    if((int)len>MINIMA_MAX_HASH_LENGTH) throw std::runtime_error("MiniData: hash len>64: "+std::to_string(len));
    MiniData r(std::vector<uint8_t>(data+offset,data+offset+len));
    offset+=len;
    return r;
}

} // namespace minima
