/***********************************************
        File Name: utf8.h
        Author: Abby Cin
        Mail: abbytsing@gmail.com
        Created Time: 10/9/19 7:17 PM
***********************************************/

#ifndef UTF_8_H_
#define UTF_8_H_

#ifdef B0 // fuck windows
#undef B0
#endif

namespace nm
{
  class UTF8
  {
    enum Type : unsigned char
    {
      B0 = 6,
      B1 = 7,
      B2 = 5,
      B3 = 4,
      B4 = 3
    };

    enum Value : unsigned char
    {
      V0 = 0b10,
      V1 = 0b0,
      V2 = 0b110,
      V3 = 0b1110,
      V4 = 0b11110
    };

    static bool test(unsigned char n, Type width, Value expected)
    {
      n >>= width;
      return n == expected;
    }

  public:
    static bool validate(const char* s, size_t len)
    {
      int width = 0;
      bool is_first = true;
      for(size_t i = 0; i < len;)
      {
        unsigned char x = s[i];
        if(is_first)
        {
          is_first = false;
          if(test(x, B1, V1))
          {
            i += 1;
            is_first = true;
          }
          else if(test(x, B2, V2))
          {
            width = 1;
          }
          else if(test(x, B3, V3))
          {
            width = 2;
          }
          else if(test(x, B4, V4))
          {
            width = 3;
          }
          else
          {
            return false;
          }
        }
        else
        {
          while(width-- && i + 1 < len)
          {
            i += 1;
            x = s[i];
            if(!test(x, B0, V0))
            {
              return false;
            }
          }

          i += 1;
          is_first = true;
          if(i >= len)
          {
            break;
          }
        }
      }
      return true;
    }
  };
}

#endif // UTF_8_H_
