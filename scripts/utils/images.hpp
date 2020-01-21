#pragma once

template <typename ComponentType>
void flipImageYAxis(
    size_t width, size_t height, size_t numComponent, ComponentType *pixels)
{
  auto *pFirstLine = pixels;
  auto *pLastLine = pixels + (height - 1) * width * numComponent;

  while (pFirstLine < pLastLine) {
    for (size_t x = 0; x < width * numComponent; ++x)
      std::swap(pFirstLine[x], pLastLine[x]);
    pFirstLine += width * numComponent;
    pLastLine -= width * numComponent;
  }
}