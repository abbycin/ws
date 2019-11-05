/*********************************************************
          File Name: header.h
          Author: Abby Cin
          Mail: abbytsing@gmail.com
          Created Time: Sat 29 Jun 2019 09:20:21 AM CST
**********************************************************/

#ifndef SHA1_H_
#define SHA1_H_

#include <string>

namespace nm
{
  // take from chromium/base/sha1_portable.cc
  class Sha1
  {
    enum
    {
      DIGEST_SIZE = 20
    };

  public:
    Sha1() { this->init(); }

    // for resuing code
    void init()
    {
      A = 0;
      B = 0;
      C = 0;
      D = 0;
      E = 0;
      cursor = 0;
      l = 0;
      H[0] = 0x67452301;
      H[1] = 0xefcdab89;
      H[2] = 0x98badcfe;
      H[3] = 0x10325476;
      H[4] = 0xc3d2e1f0;
    }

    void update(const void* data, size_t nbytes)
    {
      const uint8_t* d = reinterpret_cast<const uint8_t*>(data);
      while(nbytes--)
      {
        M[cursor++] = *d++;
        if(cursor >= 64)
        {
          process();
        }
        l += 8;
      }
    }

    void final()
    {
      pad();
      process();

      for(int t = 0; t < 5; ++t)
      {
        swapends(&H[t]);
      }
    }

    const unsigned char* digest() const { return reinterpret_cast<const unsigned char*>(H); }

    static std::string sha1(const std::string& str)
    {
      char hash[DIGEST_SIZE];
      sha1(reinterpret_cast<const unsigned char*>(str.c_str()), str.size(), reinterpret_cast<unsigned char*>(hash));
      return std::string{hash, DIGEST_SIZE};
    }

    static void sha1(const unsigned char* data, size_t len, unsigned char* res)
    {
      Sha1 sh;
      sh.update(data, len);
      sh.final();
      ::memcpy(res, sh.digest(), DIGEST_SIZE);
    }

  private:
    uint32_t A, B, C, D, E;
    uint32_t H[5];
    uint32_t cursor;
    uint32_t l;
    union
    {
      uint32_t W[80];
      uint8_t M[64];
    };

    void pad()
    {
      M[cursor++] = 0x80;

      if(cursor > 64 - 8)
      {
        // pad out to next block
        while(cursor < 64)
        {
          M[cursor++] = 0;
        }

        process();
      }

      while(cursor < 64 - 4)
      {
        M[cursor++] = 0;
      }

      M[64 - 4] = (l & 0xff000000) >> 24;
      M[64 - 3] = (l & 0xff0000) >> 16;
      M[64 - 2] = (l & 0xff00) >> 8;
      M[64 - 1] = (l & 0xff);
    }

    void process()
    {
      uint32_t t;

      // Each a...e corresponds to a section in the FIPS 180-3 algorithm.

      // a.
      //
      // W and M are in a union, so no need to memcpy.
      // memcpy(W, M, sizeof(M));
      for(t = 0; t < 16; ++t)
      {
        swapends(&W[t]);
      }

      // b.
      for(t = 16; t < 80; ++t)
      {
        W[t] = S(1, W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16]);
      }

      // c.
      A = H[0];
      B = H[1];
      C = H[2];
      D = H[3];
      E = H[4];

      // d.
      for(t = 0; t < 80; ++t)
      {
        uint32_t TEMP = S(5, A) + f(t, B, C, D) + E + W[t] + K(t);
        E = D;
        D = C;
        C = S(30, B);
        B = A;
        A = TEMP;
      }

      // e.
      H[0] += A;
      H[1] += B;
      H[2] += C;
      H[3] += D;
      H[4] += E;

      cursor = 0;
    }

    static inline uint32_t f(uint32_t t, uint32_t B, uint32_t C, uint32_t D)
    {
      if(t < 20)
      {
        return (B & C) | ((~B) & D);
      }
      else if(t < 40)
      {
        return B ^ C ^ D;
      }
      else if(t < 60)
      {
        return (B & C) | (B & D) | (C & D);
      }
      else
      {
        return B ^ C ^ D;
      }
    }

    static inline uint32_t S(uint32_t n, uint32_t X) { return (X << n) | (X >> (32 - n)); }

    static inline uint32_t K(uint32_t t)
    {
      if(t < 20)
      {
        return 0x5a827999;
      }
      else if(t < 40)
      {
        return 0x6ed9eba1;
      }
      else if(t < 60)
      {
        return 0x8f1bbcdc;
      }
      else
      {
        return 0xca62c1d6;
      }
    }

    static inline void swapends(uint32_t* t)
    {
      *t = ((*t & 0xff000000) >> 24) | ((*t & 0xff0000) >> 8) | ((*t & 0xff00) << 8) | ((*t & 0xff) << 24);
    }
  };
}

#endif // SHA1_H_
