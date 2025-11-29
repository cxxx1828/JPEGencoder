# JPEGMaster - Software JPEG Encoder

A from-scratch implementation of the JPEG compression standard. Takes raw image data and compresses it into proper JPEG files through the full encoding pipeline - color space conversion, DCT, quantization, and Huffman encoding.

## How JPEG Compression Works

The encoder runs images through 8 stages:

**1. RGB to YCbCr conversion**
Separates brightness (Y) from color information (Cb, Cr). Human eyes are more sensitive to brightness than color, so this lets us compress color more aggressively.

```c
Y  = 0.299*R + 0.587*G + 0.114*B
Cb = -0.169*R - 0.331*G + 0.500*B + 128
Cr = 0.500*R - 0.419*G - 0.081*B + 128
```

**2. Chroma subsampling (4:2:0)**
Downsamples the color channels by 2x in both dimensions. Keeps full resolution for brightness but quarters the color data. Reduces data by ~50% with minimal perceptual loss.

**3. Split into 8×8 blocks**
Divides the image into 8×8 pixel blocks (MCUs - Minimum Coded Units). A 1920×1080 image becomes 240×135 = 32,400 blocks.

**4. Discrete Cosine Transform (DCT)**
Converts each 8×8 block from spatial domain (pixel values) to frequency domain (DCT coefficients). Most of the image energy ends up in the low frequencies at the top-left of the block.

```c
// Smooth gradient pixels become mostly low-frequency coefficients
[156, 157, 158, ...] → [1260, 12, -3, 0, 0, 0, ...]
```

**5. Quantization (lossy step)**
Divides DCT coefficients by quantization table values and rounds. High-frequency coefficients (noise and fine detail) get crushed to zero. This is where compression actually happens.

```c
// Many high-frequency values become zero
[1260, 12, -3, 0] ÷ [16, 11, 10, 16] → [79, 1, 0, 0]
```

Quality parameter controls the quantization table:
- Quality 100: minimal loss, large file
- Quality 50: balanced (default)
- Quality 10: high loss, small file

**6. Zigzag ordering**
Reorders the 8×8 block into a 1D array following a zigzag pattern from low to high frequencies. Groups all the zeros together for better compression.

**7. Huffman encoding**
Run-length encodes the AC coefficients and assigns variable-length codes based on frequency. Common symbols (like runs of zeros) get short codes, rare symbols get longer codes.

DC coefficient uses DPCM - stores the difference from the previous block's DC value instead of the absolute value.

**8. JPEG file assembly**
Wraps everything in a proper JPEG file structure with all the required markers and headers (SOI, APP0, DQT, SOF0, DHT, SOS, compressed data, EOI).

## Example Usage

Basic encoding:
```c
#include "jpeg_encoder.h"

uint8_t *image = load_image("input.bmp", &width, &height);

JPEGEncoder enc;
jpeg_encoder_init(&enc, width, height, 75);  // quality 75
jpeg_encoder_set_input(&enc, image);
jpeg_encoder_encode(&enc);
jpeg_encoder_write_file(&enc, "output.jpg");
jpeg_encoder_free(&enc);
```

Compare different quality levels:
```c
int quality_levels[] = {10, 25, 50, 75, 90, 100};

for (int i = 0; i < 6; i++) {
    JPEGEncoder enc;
    jpeg_encoder_init(&enc, width, height, quality_levels[i]);
    jpeg_encoder_set_input(&enc, image);
    jpeg_encoder_encode(&enc);
    
    char filename[50];
    sprintf(filename, "output_q%d.jpg", quality_levels[i]);
    jpeg_encoder_write_file(&enc, filename);
    
    printf("Quality %d: %ld bytes\n", 
           quality_levels[i], jpeg_encoder_get_size(&enc));
    
    jpeg_encoder_free(&enc);
}
```

Batch process a directory:
```c
DIR *dir = opendir(input_dir);
struct dirent *entry;

while ((entry = readdir(dir)) != NULL) {
    if (strstr(entry->d_name, ".bmp") || strstr(entry->d_name, ".raw")) {
        // Load, encode, save
        uint8_t *image = load_image(input_path, &width, &height);
        
        JPEGEncoder enc;
        jpeg_encoder_init(&enc, width, height, 80);
        jpeg_encoder_set_input(&enc, image);
        jpeg_encoder_encode(&enc);
        jpeg_encoder_write_file(&enc, output_path);
        jpeg_encoder_free(&enc);
    }
}
```

## Project Structure

```
JPEGMaster/
├── src/
│   ├── main.c
│   ├── color_convert.c       # RGB to YCbCr
│   ├── chroma_subsample.c    # 4:2:0 downsampling
│   ├── dct.c                 # DCT transform
│   ├── quantization.c        # Quantization tables
│   ├── zigzag.c              # Zigzag reordering
│   ├── huffman.c             # Huffman encoding
│   └── jpeg_writer.c         # File assembly
├── tables/
│   ├── quant_tables.h        # Standard quantization tables
│   ├── huffman_tables.h      # Standard Huffman tables
│   └── zigzag_pattern.h
├── test/
│   ├── test_images/
│   ├── expected_output/
│   └── test_encoder.c
├── docs/
│   ├── jpeg_spec.pdf         # ITU-T T.81 standard
│   └── usage_guide.md
└── examples/
    ├── basic_usage.c
    ├── quality_comparison.c
    └── batch_encode.c
```

## Performance

Typical compression ratios at quality 75:
- Lena 512×512: 768 KB → 62 KB (12:1 ratio, 38.2 dB PSNR)
- 1920×1080 photo: 6.22 MB → 480 KB (13:1 ratio)
- Text document: tends to compress worse, maybe 4:1

Encoding speed on i7-10700K:
- 640×480: ~18ms
- 1280×720: ~45ms
- 1920×1080: ~95ms
- 3840×2160: ~380ms

Quality vs file size for a 1920×1080 image:
- Q=10: ~50 KB (visible artifacts)
- Q=25: ~120 KB (some artifacts)
- Q=50: ~280 KB (minimal artifacts)
- Q=75: ~480 KB (near-lossless)
- Q=90: ~750 KB (excellent)
- Q=100: ~1.2 MB (best possible)

## Testing

Run the test suite:
```bash
cd test/
gcc test_encoder.c ../src/*.c -o test_jpeg -lm
./test_jpeg
```

Output:
```
Running JPEG Encoder Tests...
[✓] Color conversion test passed
[✓] DCT test passed (max error: 0.01)
[✓] Quantization test passed
[✓] Zigzag ordering test passed
[✓] Huffman encoding test passed
[✓] Full encoding pipeline test passed
[✓] Output file valid JPEG format
```

## Technical Details

- Standard: ISO/IEC 10918-1 (JPEG)
- Color space: RGB input, YCbCr internal
- Subsampling: 4:2:0, 4:2:2, 4:4:4 supported
- DCT: Fast DCT using Chen-Wang algorithm
- Entropy coding: Huffman (standard tables)
- Quality range: 1-100
- Max resolution: 65535×65535
- Output: JFIF 1.02 compliant

## What Could Be Added

- Progressive JPEG encoding
- Arithmetic coding option
- SIMD optimizations (AVX2/NEON)
- Multi-threaded encoding
- GPU acceleration
- Adaptive quantization based on image content
- Python/C++ bindings
- WebAssembly port

## Author

Nina Dragićević

## License

MIT
