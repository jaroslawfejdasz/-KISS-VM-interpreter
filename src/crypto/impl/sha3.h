#pragma once
// SHA-3 (Keccak) — public domain single-header
// Based on the reference implementation by Markku-Juhani O. Saarinen
// https://github.com/mjosaarinen/tiny_sha3 (MIT/Public Domain)
#include <cstdint>
#include <cstring>
#include <cstddef>

namespace minima::crypto::impl {

static const uint64_t keccakf_rndc[24] = {
    0x0000000000000001ULL,0x0000000000008082ULL,0x800000000000808aULL,0x8000000080008000ULL,
    0x000000000000808bULL,0x0000000080000001ULL,0x8000000080008081ULL,0x8000000000008009ULL,
    0x000000000000008aULL,0x0000000000000088ULL,0x0000000080008009ULL,0x000000008000000aULL,
    0x000000008000808bULL,0x800000000000008bULL,0x8000000000008089ULL,0x8000000000008003ULL,
    0x8000000000008002ULL,0x8000000000000080ULL,0x000000000000800aULL,0x800000008000000aULL,
    0x8000000080008081ULL,0x8000000000008080ULL,0x0000000080000001ULL,0x8000000080008008ULL
};
static const int keccakf_rotc[24] = {
    1,3,6,10,15,21,28,36,45,55,2,14,27,41,56,8,25,43,62,18,39,61,20,44
};
static const int keccakf_piln[24] = {
    10,7,11,17,18,3,5,16,8,21,24,4,15,23,19,13,12,2,20,14,22,9,6,1
};

inline void keccakf(uint64_t st[25]) {
    uint64_t t, bc[5];
    for (int r = 0; r < 24; r++) {
        // Theta
        for (int i=0;i<5;i++) bc[i]=st[i]^st[i+5]^st[i+10]^st[i+15]^st[i+20];
        for (int i=0;i<5;i++){
            t=bc[(i+4)%5]^((bc[(i+1)%5]<<1)|(bc[(i+1)%5]>>63));
            for(int j=0;j<25;j+=5) st[j+i]^=t;
        }
        // Rho Pi
        t=st[1];
        for(int i=0;i<24;i++){
            int j=keccakf_piln[i];
            bc[0]=st[j];
            int rc=keccakf_rotc[i];
            st[j]=(t<<rc)|(t>>(64-rc));
            t=bc[0];
        }
        // Chi
        for(int j=0;j<25;j+=5){
            for(int i=0;i<5;i++) bc[i]=st[j+i];
            for(int i=0;i<5;i++) st[j+i]^=(~bc[(i+1)%5])&bc[(i+2)%5];
        }
        // Iota
        st[0]^=keccakf_rndc[r];
    }
}

// outlen in bytes (28=SHA3-224, 32=SHA3-256, 48=SHA3-384, 64=SHA3-512)
inline void sha3(const uint8_t* in, size_t inlen, uint8_t* out, int outlen) {
    uint64_t st[25] = {};
    uint8_t  temp[144];
    int rsiz = 200 - 2 * outlen;  // rate in bytes

    size_t i = 0;
    for (; inlen >= (size_t)rsiz; in += rsiz, inlen -= rsiz) {
        for (int j = 0; j < rsiz; j++) temp[j] = in[j];
        for (int j = 0; j < rsiz/8; j++) {
            uint64_t t = 0;
            for (int k = 0; k < 8; k++) t |= ((uint64_t)temp[j*8+k]) << (8*k);
            st[j] ^= t;
        }
        keccakf(st);
    }
    // Last block
    uint8_t last[144] = {};
    memcpy(last, in, inlen);
    last[inlen] = 0x06;   // SHA-3 domain separator (not Keccak-256)
    last[rsiz-1] |= 0x80;
    for (int j = 0; j < rsiz/8; j++) {
        uint64_t t = 0;
        for (int k = 0; k < 8; k++) t |= ((uint64_t)last[j*8+k]) << (8*k);
        st[j] ^= t;
    }
    keccakf(st);
    // Output
    for (int j = 0; j < outlen; j++) {
        out[j] = (uint8_t)(st[j/8] >> (8*(j%8)));
    }
}

} // namespace
