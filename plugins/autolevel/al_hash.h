// SPDX-License-Identifier: MIT
// Algorithm design after AutoLevel by Al-Asl (MIT).
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
namespace al {
struct Hash64 {
 uint64_t value;
 explicit Hash64(uint64_t seed=0):value(14695981039346656037ull^seed){}
 Hash64& byte(uint8_t x){value=(value^x)*1099511628211ull;return *this;}
 Hash64& u64(uint64_t x){for(int i=0;i<8;i++)byte(uint8_t(x>>(8*i)));return *this;}
 Hash64& text(const std::string& s){u64(s.size());for(unsigned char c:s)byte(c);return *this;}
 Hash64& real(double x){uint64_t bits;std::memcpy(&bits,&x,8);return u64(bits);}
 operator uint64_t()const{return value?value:1;}
};
inline uint64_t hash64(uint64_t a,uint64_t b){return Hash64().u64(a).u64(b);}
inline uint64_t textHash(const std::string& s){return Hash64().text(s);}
}
