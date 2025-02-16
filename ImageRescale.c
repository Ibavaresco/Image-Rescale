//==============================================================================
// Copyright (c) 2009-2010, Isaac Marino Bavaresco
// All rights reserved.
//==============================================================================
// isaacbavaresco@yahoo.com.br
//==============================================================================
//==============================================================================
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
//==============================================================================
// Returns the lesser of two values
#define min(a,b)            ((a)<(b)?(a):(b))

// Converts an integer value 'a' to a fixed-point value. In this implementation
// the value of 'a' may be 0 or 1.
#define int2fix(a)          ((a)<<15)

// Converts a floating-point value 'a' to a fixed-point value. In this implemen-
// tation 0 <= 'a' < 2.0.
#define float2fix(a)        ((unsigned short)((a)*(1<<15)))

// Multiplies two fixed-point values and yields a fixed-point value.
#define multfix(a,b)        (((unsigned long)(a)*(b))>>15)

// Multiplies a fixed-point value by an integer and shifts right to place the
// binary(mal) point where desired.
#define multfixint(a,b,c)   (((unsigned long)(a)*(b))>>(c))
//==============================================================================
// Rescales one line of pixels.
//
// Arguments:
//      ay  - pointer to the vertical accumulator's array.
//      s   - pointer to the line of source pixels.
//      wd  - destination width.
//      pws - proportion of source width.
//      pwd - proportion of destination width.
//      fh  - height fraction for this line.

int RescaleLine( unsigned short *ay, const unsigned char *s, unsigned short wd,
                 unsigned short pws, unsigned short pwd, unsigned short fh )
    {
    // The origin width fraction is
    // how much remains to finish the contribution of the origin pixels to the
    // destination pixels. If it is greater than 1.0 then one origin pixel will
    // contribute to more than one destination pixel. If it is less than 1.0 then more than one
    // origin pixel will contribute to the same destination pixel.
    unsigned short  fws;

    // The destination width fraction is
    // how much remains to finish the calculation of the destination pixel.
    unsigned short  fwd;

    // Width sub-pixel fraction. It is the fraction that the origin pixel contributes
    // to the composition of current sub-pixel (<= 1.0).
    unsigned short  fw;

    // Accumulator X.
    // Accumulates the contributions of origin pixels to the composition of each destination pixel.
    unsigned short  ax;

    // X coordinate of current origin pixel.
    unsigned short  xs;

    // X coordinate of current destination pixel.
    unsigned short  xd;

    // Iterate for all destination pixels of one line.
    for( ax = 0, xs = 0, xd = 0, fws = pws, fwd = pwd; xd < wd; )
        {
        // Obtain the width fraction that the origin pixel contributes to the composition of current destination sub-pixel.
        // It is always the lesser among the remaining width fractions of origin and destination (<= 1.0).
        fw  = min( fws, fwd );
        //----------------------------------------------------------------------
        // Calculate the intensity fraction that the origin pixel contributes to the horizontal composition of the destination sub-pixel,
        // multiplying the width fraction by the value of the source pixel, and add it to accumulator X.
        ax  += multfixint( fw, s[xs], 7 );
        //----------------------------------------------------------------------
        // Subtract the just used width-fraction from the remaining origin width-fraction, and if this reached zero...
        if(( fws -= fw ) == int2fix( 0 ))
            {
            // ... the contribution of this origin pixel is finished, reload the total value for the next origin pixel.
            fws      = pws;
            // Advance to the next origin pixel.
            xs++;
            }

        // Subtract the just used width-fraction from the remaining destination width-fraction, and if this reached zero...
        if(( fwd -= fw ) == int2fix( 0 ))
            {
            // ... the calculation of this destination pixel is finished, reload the total value for the next destination pixel.
            fwd      = pwd;
            // Calculate the intensity fraction that the origin pixels contribute to the vertical composition of the destination sub-pixel,
            // multiplying the height-fraction by the value of accumulator X, and add to accumulator Y.
            ay[xd]  += multfixint( fh, ax, 15 );
            // Zero the accumulator X because we are starting a new destination pixel.
            ax       = 0;
            // Advance to the next destination pixel.
            xd++;
            }
        }
    return 0;
    }
//==============================================================================
// Rescale the entire image.
//
// Arguments:
//      d   - pointer to the buffer to receive the resulting image. It must be
//            pre-allocated.
//      s   - pointer to the buffer with the original image.
//      wd  - width of the resulting image in pixels.
//      hd  - height of the resulting image in pixels.
//      ws  - width of the original image in pixels.
//      hs  - height of the original image in pixels.
//
// Return:
//      0   - success.
//      1   - failure.
//
// Note:
//      1) This version works for raw 8-bit gray-scale images. That is, the data
//         is just a bi-dimensional array of 8-bit values representing pixels with
//         256 levels of gray.
//      2) The scaling factors may be from 0.5 to 2.0 and may be different for X
//         and Y directions.

int Rescale( unsigned char *d, unsigned char *s,
             unsigned short wd, unsigned short hd,
             unsigned short ws, unsigned short hs )
    {
    unsigned short  pws;    // Width proportion at origin ( >= 0.5 && < 2.0 ).
    unsigned short  pwd;    // Width proportion at destination ( = 1.0 ).

    unsigned short  phs;    // Height proportion at origin ( >= 0.5 && < 2.0 ).
    unsigned short  phd;    // Height proportion at destination ( = 1.0 ).
    unsigned short  fhs;    // Height residue (fraction) at origin ( > 0.0 && <= 1.0 ).
    unsigned short  fhd;    // Height residue (fraction) at destination ( > 0.0 && <= 1.0 ).
    unsigned short  fh;     // Width residue (fraction) of current sub-pixel ( > 0.0 && <= 1.0 ).

    // Pointer to the vertical accumulator array. One entry for each destination column.
    // This is needed because each destination line may be composed of values from more
    // than one source line.
    unsigned short  *ay;

    // Current source line.
    unsigned short  ys;

    // Current destination line.
    unsigned short  yd;

    // Variable to iterate through all of the columns of a destination line.
    unsigned short  xd;

    // One of the dimensions is invalid...
    if( ws == 0 || hs == 0 || wd == 0 || hd == 0 )
        // ... return an error.
        return -1;

    // The origin and destination dimensions are identical...
    if( ws == wd && hs == hd )
        // ... nothing to do.
        return 0;

    // The scaling factors are outside the allowed range...
    if( (float)ws / (float)wd >= 2.0 || (float)wd / (float)ws >= 2.0 ||
        (float)hs / (float)hd >= 2.0 || (float)hd / (float)hs >= 2.0 )
        // ... return failure status.
        return -1;

    // Allocate a buffer to accumulate the partial vertical contributions
    // of the source pixels to each pixel in a destination line.
    if(( ay = (unsigned short*)malloc( wd * sizeof ay[0] )) == NULL )
        // Not enough memory, return failure status.
        return -1;

    // Calculate the width proportions of origin and destination.

    // Width proportion at origin.
    pws = float2fix( (float)wd / (float)ws );
    // Width proportion at destination == 1.
    pwd = int2fix( 1 );

    // Calculate the origin and destination height proportions.

    // Height proportion at origin.
    phs = float2fix( (float)hd / (float)hs );
    // Height proportion at destination == 1.0.
    phd = int2fix( 1 );

    // Initialize all the vertical accumulators to zero.
    memset( ay, 0x00, wd * sizeof ay[0] );

    // Iterate through all lines. It may iterate more than once for each source or
    // destination line.

    // 'ys' and 'yd' advance 'staggering', sometimes one advances, sometimes
    // the other advances, sometimes both advance together.

    for( ys = 0, yd = 0, fhs = phs, fhd = phd; yd < hd; )
        {
        // The height-fraction is the lesser among the source-height-fraction
        // and the destination-height-fraction.
        fh  = min( fhs, fhd );
        //----------------------------------------------------------------------
        // Calculate the partial destination line for this combination of source
        // (ys) and destination (yd) line.
        RescaleLine( ay, s + ws * ys, wd, pws, pwd, fh );
        //----------------------------------------------------------------------
        // Subtract the just-used height-fraction from the source-height-fraction,
        // and if the last reached zero...
        if(( fhs -= fh ) == int2fix( 0 ))
            {
            // ... reload it with the total value, ...
            fhs     = phs;
            // ... and advance the source line.
            ys++;
            }

        // Subtract the just-used height-fraction from the destination-height-fraction,
        // and if the last reached zero...
        if(( fhd -= fh ) == int2fix( 0 ))
            {
            // ... reload it with the total value, ...
            fhd     = phd;
            // ... copy the accumulated resulting line to its definitive location, ...
            for( xd = 0; xd < wd; xd++ )
                d[yd*wd+xd] = ay[xd] >> 8;
            // ... zero the vertical accumulators because we are starting a new destination line, ...
            memset( ay, 0x00, wd * sizeof ay[0] );
            // ... and advance the destination line.
            yd++;
            }
        }
    return 0;
    }
//==============================================================================

// The original and resulting image sizes are hard-coded to simplify the parsing
// of the command-line (and because I need just these sizes).

#if 1
#define WIDTHSRC    162
#define HEIGHTSRC   210
#define WIDTHDST    229
#define HEIGHTDST   295
#else
// Values for test only
#define WIDTHSRC    16
#define HEIGHTSRC   16
#define WIDTHDST    22
#define HEIGHTDST   22
#endif

int main( int ArgC, char *ArgV[] )
    {
    unsigned char   *imgsrc, *imgdst;
    int             ihandle;
    int             ohandle;

    if( ArgC != 3 )
        {
        printf( "\nUsage: %s <srcimg> <dstimg>\n\n", ArgV[0] );
        return -1;
        }

    if(( imgsrc = (unsigned char*)malloc( WIDTHSRC * HEIGHTSRC )) == NULL )
        {
        printf( "Not enough memory!\n" );
        goto Error0;
        }

    if(( imgdst = (unsigned char*)malloc( WIDTHDST * HEIGHTDST )) == NULL )
        {
        printf( "Not enough memory!\n" );
        goto Error1;
        }

    if(( ihandle = open( ArgV[1], O_RDONLY )) == -1 )
        {
        printf( "Could not open input file!\n" );
        goto Error2;
        }

    if( read( ihandle, imgsrc, WIDTHSRC * HEIGHTSRC ) != WIDTHSRC * HEIGHTSRC )
        {
        printf( "Could not read input file!\n" );
        goto Error3;
        }

    Rescale( imgdst, imgsrc, WIDTHDST, HEIGHTDST, WIDTHSRC, HEIGHTSRC );

    if(( ohandle = open( ArgV[2], O_WRONLY | O_CREAT | O_TRUNC )) == -1 )
        {
        printf( "Could not open output file!\n" );
        goto Error3;
        }

    if( write( ohandle, imgdst, WIDTHDST * HEIGHTDST ) != WIDTHDST * HEIGHTDST )
        {
        printf( "Could not write output file!\n" );
        goto Error4;
        }

    close( ohandle );
    close( ihandle );
    free( imgdst );
    free( imgsrc );
    return 0;

Error4:
    close( ohandle );
Error3:
    close( ihandle );
Error2:
    free( imgdst );
Error1:
    free( imgsrc );
Error0:
    return -1;
    }
//==============================================================================

