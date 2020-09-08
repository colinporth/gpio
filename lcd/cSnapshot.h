// cSnapshot.h
#pragma once
#include <cstdint>

class cSnapshot {
public:
  cSnapshot (const uint16_t width, const uint16_t height);
  ~cSnapshot();

  void snap (uint16_t* frameBuf);

private:
  const uint16_t mWidth;
  const uint16_t mHeight;
  };
