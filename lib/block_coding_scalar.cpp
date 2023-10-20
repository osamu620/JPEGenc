#include <cstdint>
#include <hwy/highway.h>
#include "zigzag_order.hpp"

//  Branchless abs:
//  https://stackoverflow.com/questions/9772348/get-absolute-value-without-using-abs-function-nor-if-statement
#define JPEGENC_ABS16(x) ((x) + ((x) >> 15)) ^ ((x) >> 15)
uint64_t bitmap;
HWY_ALIGN int16_t dp[64];

int16_t dc = sp[0];
sp[0] -= prev_dc;
// update previous dc value
prev_dc = dc;

bitmap = 0;
for (int i = 0; i < DCTSIZE2; ++i) {
  dp[i] = sp[indices[i]];
  bitmap |= (dp[i] != 0);
  bitmap <<= 1;
}
int32_t nbits;
uint32_t uval = JPEGENC_ABS16(dp[0]);
nbits         = 32 - JPEGENC_CLZ32(uval);

//  EncodeDC
enc.put_bits(tab.DC_cwd[nbits], tab.DC_len[nbits]);
if (nbits != 0) {
  dp[0] -= (dp[0] >> 15) & 1;
  enc.put_bits(dp[0] & ((1 << nbits) - 1), nbits);
}

int count              = 1;
const uint32_t ZRL_cwd = tab.AC_cwd[0xF0];
const int32_t ZRL_len  = tab.AC_len[0xF0];
const uint32_t EOB_cwd = tab.AC_cwd[0x00];
const int32_t EOB_len  = tab.AC_len[0x00];

while (bitmap != 0) {
  int run = JPEGENC_CLZ64(bitmap);
  count += run;
  bitmap <<= run;
  while (run > 15) {
    // ZRL
    enc.put_bits(ZRL_cwd, ZRL_len);
    run -= 16;
  }
  // Encode AC
  uval  = (dp[count] + (dp[count] >> 15)) ^ (dp[count] >> 15);
  nbits = 32 - JPEGENC_CLZ32(uval);
  dp[count] -= (dp[count] >> 15) & 1;
  uval = dp[count] & ((1 << nbits) - 1);

  int32_t RS = (run << 4) + nbits;
  uval |= tab.AC_cwd[RS] << nbits;
  nbits += tab.AC_len[RS];
  enc.put_bits(uval, nbits);

  count++;
  bitmap <<= 1;
}
if (count != 64) {
  // EOB
  enc.put_bits(EOB_cwd, EOB_len);
}