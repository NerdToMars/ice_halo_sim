#include <chrono>
#include <cstdio>
#include <unordered_map>

#include <opencv2/opencv.hpp>

#include "render.h"
#include "context.h"

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::printf("USAGE: %s config.json\n", argv[0]);
    return -1;
  }

  auto start = std::chrono::system_clock::now();
  IceHalo::RenderContextPtr ctx = IceHalo::RenderContext::CreateFromFile(argv[1]);
  IceHalo::SpectrumRenderer renderer(ctx);
  renderer.LoadData();

  auto flat_rgb_data = new uint8_t[3 * ctx->GetImageWidth() * ctx->GetImageHeight()];
  renderer.RenderToRgb(flat_rgb_data);

  cv::Mat img(ctx->GetImageHeight(), ctx->GetImageWidth(), CV_8UC3, flat_rgb_data);
  cv::cvtColor(img, img, cv::COLOR_RGB2BGR);
  try {
    cv::imwrite(ctx->GetImagePath(), img);
  } catch (cv::Exception& ex) {
    fprintf(stderr, "Exception converting image to PNG format: %s\n", ex.what());
    delete[] flat_rgb_data;
    return -1;
  }
  delete[] flat_rgb_data;

  auto t1 = std::chrono::system_clock::now();
  std::chrono::duration<float, std::ratio<1, 1000> > diff = t1 - start;
  std::printf("Total: %.2fms\n", diff.count());
}
