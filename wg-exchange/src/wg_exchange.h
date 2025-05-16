#pragma once

#define GETTER(T, N) \
  T get_##N() const { return N; }

#define GETTER_SETTER(T, N) \
  GETTER(T, N)              \
  void set_##N(T x) { N = x; }

#define WG_KEY_LEN 32
#define WG_KEY_LEN_BASE64 ((((WG_KEY_LEN) + 2) / 3) * 4 + 1)

// Using the same base64 encoding implementaion as in wireguard-tools
inline void encode_base64(char dest[4], const uint8_t src[3]) {
  const uint8_t input[] = {
      static_cast<uint8_t>((src[0] >> 2) & 63),
      static_cast<uint8_t>(((src[0] << 4) | (src[1] >> 4)) & 63),
      static_cast<uint8_t>(((src[1] << 2) | (src[2] >> 6)) & 63),
      static_cast<uint8_t>(src[2] & 63)};
  for (unsigned int i = 0; i < 4; ++i)
    dest[i] = input[i] + 'A'
          + (((25 - input[i]) >> 8) & 6)
          - (((51 - input[i]) >> 8) & 75)
          - (((61 - input[i]) >> 8) & 15)
          + (((62 - input[i]) >> 8) & 3);
}

inline void key_to_base64(char base64[WG_KEY_LEN_BASE64],
                          const uint8_t key[WG_KEY_LEN]) {
  unsigned int i;

  for (i = 0; i < WG_KEY_LEN / 3; ++i)
    encode_base64(&base64[i * 4], &key[i * 3]);
  encode_base64(&base64[i * 4],
                (const uint8_t[]){key[i * 3 + 0], key[i * 3 + 1], 0});
  base64[WG_KEY_LEN_BASE64 - 2] = '=';
  base64[WG_KEY_LEN_BASE64 - 1] = '\0';
}