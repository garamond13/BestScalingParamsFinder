#include "common.h"
#include "cxxopts.hpp"
#include "engine.h"
#include "global.h"
#include "config.h"

#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define FLT_EPS 1e-6f

struct Best_result
{
    float radius;
    float blur;
    float p1;
    float p2;
    double result;
};

int main(int argc, char** argv)
{
    cxxopts::Options options("BestScalingParamsFinder v1.0.0");

    options.add_options()
        ("h,help", "Print help")
        ("ref-img", "Rescaled image will be compared to this reference image", cxxopts::value<std::string>()->default_value(""))
        ("scld-img", "Scaled image that will be rescaled to the reference image size", cxxopts::value<std::string>()->default_value(""))
        ("filter", "Filter index: 0 - Orthogonal (Sinc), 1 - Cylindrical (Jinc)", cxxopts::value<int>()->default_value("0"))
        ("kernel", "Kernel index: 0 - Lanczos, 1 - Ginseng, 2 - Hamming, 3 - PowCosine, 4 - Kaiser, 5 - PowGaramond, 6 - PowBlackman, 7 - GNW, 8 - Said, 9 - Bicubic, 10 - FSR, 11 - BCSpline", cxxopts::value<int>()->default_value("0"))
        ("radius-lo", "Kernel radius low value", cxxopts::value<float>()->default_value("2.0"))
        ("radius-hi", "Kernel radius high value", cxxopts::value<float>()->default_value("2.0"))
        ("radius-i", "Kernel radius increment", cxxopts::value<float>()->default_value("0.0"))
        ("blur-lo", "Kernel blur low value (0.0, inf)", cxxopts::value<float>()->default_value("1.0"))
        ("blur-hi", "Kernel blur high value (0.0, inf)", cxxopts::value<float>()->default_value("1.0"))
        ("blur-i", "Kernel blur increment", cxxopts::value<float>()->default_value("0.0"))
        ("p1-lo", "First free kernel parameter low value", cxxopts::value<float>()->default_value("0.0"))
        ("p1-hi", "First free kernel parameter high value", cxxopts::value<float>()->default_value("0.0"))
        ("p1-i", "First free kernel parameter increment", cxxopts::value<float>()->default_value("0.0"))
        ("p2-lo", "Second free kernel parameter low value", cxxopts::value<float>()->default_value("0.0"))
        ("p2-hi", "Second free kernel parameter high value", cxxopts::value<float>()->default_value("0.0"))
        ("p2-i", "Second free kernel parameter increment", cxxopts::value<float>()->default_value("0.0"))
        ("ar", "Antiringing strenght [0.0, 1.0]", cxxopts::value<float>()->default_value("1.0"))
        ;

    auto result = options.parse(argc, argv);

    // Handle help.
    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    // Read config.
    // We only do some basic value limits.
    config::reference_img = result["ref-img"].as<std::string>();
    config::scaled_img = result["scld-img"].as<std::string>();
    config::filter = std::clamp(result["filter"].as<int>(), 0, 1);
    config::kernel = std::clamp(result["kernel"].as<int>(), 0, 11);
    config::radius_lo = std::max(result["radius-lo"].as<float>(), FLT_EPS);
    config::radius_hi = std::max(result["radius-hi"].as<float>(), config::radius_lo);
    config::radius_i = std::max(result["radius-i"].as<float>(), 0.0f);
    config::blur_lo = std::max(result["blur-lo"].as<float>(), FLT_EPS);
    config::blur_hi = std::max(result["blur-hi"].as<float>(), config::blur_lo);
    config::blur_i = std::max(result["blur-i"].as<float>(), 0.0f);
    config::p1_lo = result["p1-lo"].as<float>();
    config::p1_hi = std::max(result["p1-hi"].as<float>(), config::p1_lo);
    config::p1_i = std::max(result["p1-i"].as<float>(), 0.0f);
    config::p2_lo = result["p2-lo"].as<float>();
    config::p2_hi = std::max(result["p2-hi"].as<float>(), config::p2_lo);
    config::p2_i = std::max(result["p2-i"].as<float>(), 0.0f);
    config::ar = std::clamp(result["ar"].as<float>(), 0.0f, 1.0f);

    // Load images.
    int n;
    auto* scaled_image_data = stbi_load(config::scaled_img.c_str(), &g_src_width, &g_src_height, &n, 0);
    if (!scaled_image_data) {
        std::cerr << stbi_failure_reason();
        return 1;
    }
    if (n != 1) {
        std::cerr << "ERROR: Scaled image has to be 1 channel greyscale.\n";
        return 1;
    }
    g_reference_image_data = stbi_load(config::reference_img.c_str(), &g_dst_width, &g_dst_height, &n, 0);
    if (!g_reference_image_data) {
        std::cerr << stbi_failure_reason();
        return 1;
    }
    if (n != 1) {
        std::cerr << "ERROR: Reference image has to be 1 channel greyscale.\n";
        return 1;
    }

    // Prepare engine.
    Engine engine;
    engine.init();
    engine.create_image(scaled_image_data);
    engine.scale = static_cast<float>(g_dst_width) / static_cast<float>(g_src_width);

    Best_result best_result = {};
    std::cout << std::fixed;
    
    // Mian loop.
    for (g_kernel_radius = config::radius_lo; g_kernel_radius < config::radius_hi + FLT_EPS; g_kernel_radius += config::radius_i) {
        for (g_kernel_blur = config::blur_lo; g_kernel_blur < config::blur_hi + FLT_EPS; g_kernel_blur += config::blur_i) {
            for (g_kernel_parameter1 = config::p1_lo; g_kernel_parameter1 < config::p1_hi + FLT_EPS; g_kernel_parameter1 += config::p1_i) {
                for (g_kernel_parameter2 = config::p2_lo; g_kernel_parameter2 < config::p2_hi + FLT_EPS; g_kernel_parameter2 += config::p2_i) {
                    engine.resample_image();
                    auto result = engine.compare();

                    // Print current result.
                    std::cout << std::setprecision(6);
                    std::cout << "R: " << g_kernel_radius;
                    std::cout << ", B: " << g_kernel_blur;
                    std::cout << ", P1: " << g_kernel_parameter1;
                    std::cout << ", P2: " << g_kernel_parameter2;
                    std::cout << std::setprecision(15);
                    std::cout << ", SSIM: " << result << "\n";

                    // Save the best result.
                    if (best_result.result < result) {
                        best_result.radius = g_kernel_radius;
                        best_result.blur = g_kernel_blur;
                        best_result.p1 = g_kernel_parameter1;
                        best_result.p2 = g_kernel_parameter2;
                        best_result.result = result;
                    }
                    
                    // Prevent infinite loop.
                    if (!config::p2_i) {
                        break;
                    }
                }

                // Prevent infinite loop.
                if (!config::p1_i) {
                    break;
                }
            }
            
            // Prevent infinite loop.
            if (!config::blur_i) {
                break;
            }
        }

        // Prevent infinite loop.
        if (!config::radius_i) {
            break;
        }
    }

    // Print the best result.
    std::cout << "The best: ";
    std::cout << std::setprecision(6);
    std::cout << "R: " << best_result.radius;
    std::cout << ", B: " << best_result.blur;
    std::cout << ", P1: " << best_result.p1;
    std::cout << ", P2: " << best_result.p2;
    std::cout << std::setprecision(15);
    std::cout << ", SSIM: " << best_result.result << "\n";

    stbi_image_free(scaled_image_data);
    stbi_image_free(g_reference_image_data);
    return 0;
}