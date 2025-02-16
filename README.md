Fixed-point image rescaling algorithm

by Isaac Marino Bavaresco

<pre>
I developed this algorithm to run efficiently on platforms without hardware floating-point support. Besides using fixed-point math it uses a different approach than any others I have seen.

It can resize the image by factors from 0 to 2 (exclusive), where 1 keeps the original size. The scale factors for the X and Y directions can be different.

The upper-limit of the scaling factor may be increased by increasing the number of integer bits in the fixed-point format. I am working on a version that selects the number of integer- and fractional-part bits depending on the scaling factors.

I think I invented this algorithm (that is, I have never seen it anywhere else before), but perhaps it was already invented by someone else. If you know of an existing source for this algorithm, please let me know.

Here is how the rescaling is done for one line of pixels (width) in a gray-scale image (The rescaling on the height follows the same principle):

First, let's imagine that both images have the same width (units of measure) but different number of pixels, so the pixels of the image with less pixels are wider than the pixels of the image with more pixels.

The width of the pixels of the destination image is always 1.0, so if we are enlarging the image (increasing the number of pixels), then the width of the pixels of the source image is greater than 1.0. If we are shrinking (reducing the number of pixels), then the width of the pixels of the source image is smaller than 1.0.

Let's suppose we want to enlarge one line from 3 pixels to 5 pixels. We must pair both the source and destination pixels: (fig. 1)

After pairing, we must imagine additional borders inside pixels (dotted lines), mirroring the borders between the pixels of the other line (source or destination), effectively creating sub-pixels. (fig. 2)

The width of each sub-pixel is the width of the intersection between the source and destination pixels that it covers.

The value of each destination pixel is the sum of one or more sub-pixels. The contribution of each sub-pixel to the intensity of the destination pixel is the sub-pixel width multiplied by the intensity of the source pixel that contains it.

For instance, destination pixel D1 is composed by only one sub-pixel (S1 * 1.0). Destination pixel D2 is composed by two sub-pixels ( S1 * 0.66 + S2 * 0.33 ). And so on.

The sum of the widths of all sub-pixels that compose each destination pixel is always 1.0, so we don't need to do a division, just sum the products of the sub-pixel widths by the origin pixel values.

Algorithmically:

fws = 1.66666;// Width of the source pixel
fwd = 1.0;    // Width of the destination pixel
acc = 0;      // Accumulator
si  = 0;      // Source pixel index
di  = 0;      // Destination pixel index

while( di < dstimgwidth ){
    fw  = min( fws, fwd ); // Sub-pixel width
    acc += srcimg[si] * fw;// Add the sub-pixel contribution

    fws -= fw;             // Subtract the already used portion of the source pixel

    if( fws == 0 ){        // Current source pixel exausted
        fws = 1.66666;     // Reload source pixel width
        si++;              // Next source pixel
    }

    fwd -= fw;             // Subtract the already used portion of the destination pixel

    if( fwd == 0 ){        // Finished calculation of current destination pixel 
        fwd = 1.0;         // Reload destination pixel width
        dstimg[di] = acc;  // Save calculated destination pixel
        di++;              // Next destination pixel
        acc = 0;           // Zero accumulator for next destination pixel
    }
}
</pre>
