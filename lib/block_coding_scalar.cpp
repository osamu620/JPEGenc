//
// Created by OSAMU WATANABE on 2023/08/05.
//

#include "zigzag_order.hpp"
for (int i = 0; i < DCTSIZE2; ++i) {
  dp[i] = sp[scan[i]];
  bitmap |= (dp[i] != 0);
  bitmap <<= 1;
}
int32_t s;
//  Branchless abs:
//  https://stackoverflow.com/questions/9772348/get-absolute-value-without-using-abs-function-nor-if-statement
uint32_t uval = (dp[0] + (dp[0] >> 15)) ^ (dp[0] >> 15);
s             = 32 - JPEGENC_CLZ32(uval);

//  EncodeDC
enc.put_bits(tab.DC_cwd[s], tab.DC_len[s]);
if (s != 0) {
  dp[0] -= (dp[0] >> 15) & 1;
  enc.put_bits(dp[0], s);
}

int count = 1;
int run;
while (bitmap != 0) {
  run = JPEGENC_CLZ64(bitmap);
  count += run;
  bitmap <<= run;
  while (run > 15) {
    // ZRL
    enc.put_bits(tab.AC_cwd[0xF0], tab.AC_len[0xF0]);
    run -= 16;
  }
  // Encode AC
  uval = (dp[count] + (dp[count] >> 15)) ^ (dp[count] >> 15);
  s    = 32 - JPEGENC_CLZ32(uval);
  enc.put_bits(tab.AC_cwd[(run << 4) + s], tab.AC_len[(run << 4) + s]);
  dp[count] -= (dp[count] >> 15) & 1;
  enc.put_bits(dp[count], s);
  count++;
  bitmap <<= 1;
}
if (count != 64) {
  // EOB
  enc.put_bits(tab.AC_cwd[0x00], tab.AC_len[0x00]);
}