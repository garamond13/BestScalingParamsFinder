# BestScalingParamsFinder
**BestScalingParamsFinder** automates the process of finding optimal parameters for image scaling kernels. It resizes the scaled input image to the reference input image dimensions using range of parameter combinations, and evaluates the quality of each result by computing the Structural Similarity Index (SSIM) between the rescaled image and the reference image.

Uses [Chris Lomont SSIM implementation](https://github.com/ChrisLomont/SSIM).

## Usage
Input images are, scaled image that will be rescaled to reference image size, and the reference image.  
Images need to be 1 channel greyscale sRGB.  
Use `-h` or `--help` to print help about all options.  
If you want to lock any of parameters set "-lo" and "-hi" to the same value and set "-i" to 0.0.  

Example usage and output:
```
BestScalingParamsFinder.exe --ref-img ref.png --scld-img scld.png --kernel 9 --p1-lo -1.0 --p1-hi 0.0 --p1-i 0.1  
R: 2.000000, B: 1.000000, P1: -1.000000, P2: 0.000000, SSIM: 0.982032554157652  
R: 2.000000, B: 1.000000, P1: -0.900000, P2: 0.000000, SSIM: 0.986954348755108  
R: 2.000000, B: 1.000000, P1: -0.800000, P2: 0.000000, SSIM: 0.990853317338708  
R: 2.000000, B: 1.000000, P1: -0.700000, P2: 0.000000, SSIM: 0.993772010927650  
R: 2.000000, B: 1.000000, P1: -0.600000, P2: 0.000000, SSIM: 0.995797642013453  
R: 2.000000, B: 1.000000, P1: -0.500000, P2: 0.000000, SSIM: 0.997040504634779  
R: 2.000000, B: 1.000000, P1: -0.400000, P2: 0.000000, SSIM: 0.997646986076961  
R: 2.000000, B: 1.000000, P1: -0.300000, P2: 0.000000, SSIM: 0.997776222153276  
R: 2.000000, B: 1.000000, P1: -0.200000, P2: 0.000000, SSIM: 0.997583871415808  
R: 2.000000, B: 1.000000, P1: -0.100000, P2: 0.000000, SSIM: 0.997218789477676  
R: 2.000000, B: 1.000000, P1: 0.000000, P2: 0.000000, SSIM: 0.996792182037212  
The best: R: 2.000000, B: 1.000000, P1: -0.300000, P2: 0.000000, SSIM: 0.997776222153276  
```

## Special notes on some kernels and windows

**Power of Cosine**  
n = p1  
Has to be satisfied: n >= 0  
n = 0: Box window  
n = 1: Cosine window  
n = 2: Hann window  

**Kaiser**  
beta = p1  

**Power of Garamond**  
n = p1, m = p2  
Has to be satisfied: n >= 0, m >= 0  
n = 0: Box window  
m = 0: Box window  
n -> inf, m <= 1: Box window  
m = 1: Garamond window  
n = 1, m = 1: Linear window  
n = 2, m = 1: Welch window  

**Power of Blackman**  
a = p1, n = p2  
Has to be satisfied: n >= 0  
Has to be satisfied: if n != 1, a <= 0.16  
n = 0: Box window  
n = 1: Blackman window  
a = 0, n = 1: Hann window  
a = 0, n = 0.5: Cosine window  

**Generalized Normal Window (GNW)**  
s = p1, n = p2  
Has to be satisfied: s != 0, n >= 0  
n -> inf: Box window  
n = 2: Gaussian window  

**Said**  
eta = p1, chi = p2  
Has to be satisfied: eta != 2  

**Bicubic**  
a = p1
Has to be satisfied: radius = 2  
Blur is unused.  

**Modified FSR Kernel**  
b = p1, c = p2  
Has to be satisfied: radius = 2, b != 0, b != 2, c != 0  
Blur is unused.   
c = 1: FSR kernel  

**BC-Spline**  
b = p1, c = p2  
Has to be satisfied: radius = 2  
Blur is unused.
Keys kernels: B + 2C = 1  
B = 1, C = 0: Spline kernel  
B = 0, C = 0: Hermite kernel  
B = 0, C = 0.5: Catmull-Rom kernel  
B = 1 / 3, C = 1 / 3: Mitchell kernel  
B = 12 / (19 + 9 * sqrt(2)), C = 113 / (58 + 216 * sqrt(2)): Robidoux kernel  
B = 6 / (13 + 7 * sqrt(2)), C = 7 / (2 + 12 * sqrt(2)): RobidouxSharp kernel  
B = (9 - 3 * sqrt(2)) / 7, C = 0.1601886205085204: RobidouxSoft kernel  

## Compilation
You can use Visual Studio.  
No additional dependencies.  