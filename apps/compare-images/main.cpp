#include <args.hxx>
#include <cmath>
#include <fstream>
#include <iostream>
#include <json.hpp>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

using namespace nlohmann;

int main(int argc, char **argv)
{
  args::ArgumentParser parser{"Compare a reference image with a test image. "
                              "Exit code is zero if the images are the same."};
  args::HelpFlag help{parser, "help", "Display this help menu", {'h', "help"}};
  args::Positional<std::string> referenceImage{parser, "reference-image",
      "Path to PNG reference image", args::Options::Required};
  args::Positional<std::string> testImage{
      parser, "test-image", "Path to PNG test image", args::Options::Required};
  args::Positional<std::string> differenceRGBImage{parser,
      "difference-rgb-image",
      "Path to PNG difference RGB image. Is only computed if the test image is "
      "different from the reference image.",
      args::Options::Required};
  args::Positional<std::string> differenceAlphaImage{parser,
      "difference-alpha-image",
      "Path to PNG difference alpha image. Is only computed if the test image "
      "is different from the reference image.",
      args::Options::Required};
  args::Positional<std::string> errorJsonFile{parser, "error-json-file",
      "Path to a json file in which computed error is stored, with other "
      "statistics. Is only computyed if the test image is different from the "
      "reference image.",
      args::Options::Required};
  try {
    parser.ParseCLI(argc, argv);
  } catch (const args::Completion &e) {
    std::cout << e.what();
    return 0;
  } catch (const args::Help &) {
    std::cout << parser;
    return 0;
  } catch (const args::Error &e) {
    std::cerr << e.what() << std::endl;
    std::cerr << parser;
    return 1;
  }

  const int componentCount = 4;
  int referenceWidth = 0, referenceHeight = 0, referenceComp = 0;
  const auto referencePixels = stbi_load(referenceImage.Get().c_str(),
      &referenceWidth, &referenceHeight, &referenceComp, componentCount);

  int testWidth = 0, testHeight = 0, testComp = 0;
  const auto testPixels = stbi_load(testImage.Get().c_str(), &testWidth,
      &testHeight, &testComp, componentCount);

  if (testWidth != referenceWidth) {
    std::cerr << "testWidth != referenceWidth" << std::endl;
    return -1;
  }

  if (testHeight != referenceHeight) {
    std::cerr << "testHeight != referenceHeight" << std::endl;
    return -1;
  }

  if (testComp != referenceComp) {
    std::cerr << "testComp != referenceComp" << std::endl;
    return -1;
  }

  struct PixelDiff
  {
    int row, column, pixelIndex, comp, ref, test;
  };

  std::vector<PixelDiff> pixelDiffs;

  for (auto row = 0; row < referenceHeight; ++row) {
    for (auto column = 0; column < referenceWidth; ++column) {
      const auto pixelIndex = row * referenceWidth + column;
      const auto refColor = referencePixels + pixelIndex * componentCount;
      const auto testColor = testPixels + pixelIndex * componentCount;
      for (auto comp = 0; comp < componentCount; ++comp) {
        if (refColor[comp] != testColor[comp]) {
          pixelDiffs.emplace_back(PixelDiff{row, column, pixelIndex, comp,
              int(refColor[comp]), int(testColor[comp])});
        }
      }
    }
  }

  if (!pixelDiffs.empty()) {
    std::vector<unsigned char> diffRGBPixels(
        referenceWidth * referenceHeight * 3, 0);

    std::vector<unsigned char> diffAlphaPixels(
        referenceWidth * referenceHeight * 3, 0);

    json output;
    output["pixel_differences"] = json::array();

    for (const auto &diff : pixelDiffs) {
      const auto absDiff = std::abs(diff.ref - diff.test);

      if (diff.comp < 3) {
        diffRGBPixels[diff.pixelIndex * 3 + diff.comp] = absDiff;
      }
      if (diff.comp == 3) {
        // Only fill red component
        diffAlphaPixels[diff.pixelIndex * 3 + diff.comp] =
            std::abs(diff.ref - diff.test);
      }

      if (output["pixel_differences"].size() < 1024) {
        output["pixel_differences"].emplace_back(
            json::object({{"row", diff.row}, {"column", diff.column},
                {"comp", diff.comp}, {"ref", diff.ref}, {"test", diff.test}}));
      }
    }

    if (output["pixel_differences"].size() >= 1024) {
      std::cerr << "Number of differences is " << pixelDiffs.size()
                << ". Only outputing 1024 in json." << std::endl;
    }

    stbi_write_png(differenceRGBImage.Get().c_str(), referenceWidth,
        referenceHeight, 3, diffRGBPixels.data(), 0);
    stbi_write_png(differenceAlphaImage.Get().c_str(), referenceWidth,
        referenceHeight, 3, diffAlphaPixels.data(), 0);

    std::ofstream o(errorJsonFile.Get());
    o << output.dump(2) << std::endl;

    return -1;
  }

  std::clog << "OK." << std::endl;
  return 0;
}