#include "ViewerApplication.hpp"
#include "utils/GLFWHandle.hpp"
#include "utils/filesystem.hpp"

#include <args.hxx>

std::vector<std::string> split(
    const std::string &str, const std::string &delim);

int main(int argc, char **argv)
{
  auto returnCode = 0;

  // args library https://github.com/taywee/args
  args::ArgumentParser parser{"glTF Viewer."};
  args::HelpFlag help{parser, "help", "Display this help menu", {'h', "help"}};
  args::Group commands{parser, "commands"};
  args::Command info{commands, "info", "Display info about OpenGL",
      [&](args::Subparser &parser) {
        parser.Parse();
        GLFWHandle handle{1, 1, "", false};
        printGLVersion();
      }};
  args::Command interactive{
      commands, "viewer", "Run glTF viewer", [&](args::Subparser &parser) {
        args::Positional<std::string> file{
            parser, "file", "Path to file", args::Options::Required};
        args::ValueFlag<std::string> lookat{parser, "lookat",
            "Look at parameters for the Camera with format "
            "eye_x,eye_y,eye_z,center_x,center_y,center_z,up_x,up_y,up_z",
            {"lookat"}};
        args::ValueFlag<std::string> vertexShader{
            parser, "vs", "Vertex shader to use", {"vs"}};
        args::ValueFlag<std::string> fragmentShader{
            parser, "fs", "Fragment shader to use", {"fs"}};
        args::ValueFlag<int32_t> imageWidth{parser, "width",
            "Width of window or output image if -b is specified",
            {"w", "width"}};
        args::ValueFlag<int32_t> imageHeight{parser, "height",
            "Height of window or output image if -b is specified",
            {"h", "height"}};
        args::ValueFlag<std::string> output{parser, "output",
            "Output path to render the image. If specified no window is shown. "
            "Only png is supported.",
            {"o", "output"}};
        parser.Parse();

        std::vector<float> lookatParams;
        if (lookat) {
          const std::string &lookatArgs = args::get(lookat);
          const auto tokens = split(lookatArgs, ",");
          if (tokens.size() != 9) {
            throw args::ValidationError("Unable to parse --lookat argument "
                                        "(expected 9 numbers, got " +
                                        std::to_string(tokens.size()) + ")");
          }
          for (const auto &arg : tokens) {
            lookatParams.emplace_back(std::stof(arg));
          }
        }

        uint32_t width = imageWidth ? args::get(imageWidth) : 1280;
        uint32_t height = imageHeight ? args::get(imageHeight) : 720;

        ViewerApplication app{fs::path{argv[0]}, width, height, args::get(file),
            lookatParams, args::get(vertexShader), args::get(fragmentShader),
            args::get(output)};
        returnCode = app.run();
      }};

  try {
    parser.ParseCLI(argc, argv);
  } catch (const args::Completion &e) {
    std::cout << e.what();
    return 0;
  } catch (const args::Help &) {
    std::cout << parser;
    return 0;
  } catch (const args::ParseError &e) {
    std::cerr << e.what() << std::endl;
    std::cerr << parser;
    return 1;
  } catch (const args::RequiredError &e) {
    std::cerr << e.what() << std::endl;
    std::cerr << parser;
    return 1;
  } catch (const args::ValidationError &e) {
    std::cerr << e.what() << std::endl;
    std::cerr << parser;
    return 1;
  }

  return returnCode;
}

std::vector<std::string> split(const std::string &str, const std::string &delim)
{
  std::vector<std::string> tokens;
  size_t prev = 0, pos = 0;
  do {
    pos = str.find(delim, prev);
    if (pos == std::string::npos)
      pos = str.length();
    std::string token = str.substr(prev, pos - prev);
    if (!token.empty())
      tokens.push_back(token);
    prev = pos + delim.length();
  } while (pos < str.length() && prev < str.length());
  return tokens;
}