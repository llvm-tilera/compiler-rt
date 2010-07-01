/*
 *                     The LLVM Compiler Infrastructure
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 */

#define SINGLE_PRECISION
#include "fp_lib.h"

// This file implements single-precision soft-float multiplication with the
// IEEE-754 default rounding (to nearest, ties to even).

// 32x32 --> 64 bit multiply
static inline void wideMultiply(rep_t a, rep_t b, rep_t *hi, rep_t *lo) {
    const uint64_t product = (uint64_t)a*b;
    *hi = product >> 32;
    *lo = product;
}

fp_t __mulsf3(fp_t a, fp_t b) {
    
    const unsigned int aExponent = toRep(a) >> significandBits & maxExponent;
    const unsigned int bExponent = toRep(b) >> significandBits & maxExponent;
    const rep_t productSign = (toRep(a) ^ toRep(b)) & signBit;
    
    rep_t aSignificand = toRep(a) & significandMask;
    rep_t bSignificand = toRep(b) & significandMask;
    int scale = 0;
    
    // Detect if a or b is zero, denormal, infinity, or NaN.
    if (aExponent-1U >= maxExponent-1U || bExponent-1U >= maxExponent-1U) {
        
        const rep_t aAbs = toRep(a) & absMask;
        const rep_t bAbs = toRep(b) & absMask;
        
        // NaN * anything = qNaN
        if (aAbs > infRep) return fromRep(toRep(a) | quietBit);
        // anything * NaN = qNaN
        if (bAbs > infRep) return fromRep(toRep(b) | quietBit);
        
        if (aAbs == infRep) {
            // infinity * non-zero = +/- infinity
            if (bAbs) return fromRep(aAbs | productSign);
            // infinity * zero = NaN
            else return fromRep(qnanRep);
        }
        
        if (bAbs == infRep) {
            // non-zero * infinity = +/- infinity
            if (aAbs) return fromRep(bAbs | productSign);
            // zero * infinity = NaN
            else return fromRep(qnanRep);
        }
        
        // zero * anything = +/- zero
        if (!aAbs) return fromRep(productSign);
        // anything * zero = +/- zero
        if (!bAbs) return fromRep(productSign);
        
        // one or both of a or b is denormal, the other (if applicable) is a
        // normal number.  Renormalize one or both of a and b, and set scale to
        // include the necessary exponent adjustment.
        if (aAbs < implicitBit) scale += normalize(&aSignificand);
        if (bAbs < implicitBit) scale += normalize(&bSignificand);
    }
    
    // Or in the implicit significand bit.  (If we fell through from the
    // denormal path it was already set by normalize( ), but setting it twice
    // won't hurt anything.)
    aSignificand |= implicitBit;
    bSignificand |= implicitBit;
    
    // Get the significand of a*b.  Before multiplying the significands, shift
    // one of them left to left-align it in the field.  Thus, the product will
    // have (exponentBits + 2) integral digits, all but two of which must be
    // zero.  Normalizing this result is just a conditional left-shift by one
    // and bumping the exponent accordingly.
    rep_t productHi, productLo;
    wideMultiply(aSignificand, bSignificand << exponentBits,
                 &productHi, &productLo);
    
    int productExponent = aExponent + bExponent - exponentBias + scale;
    
    // Normalize the significand, adjust exponent if needed.
    if (productHi & implicitBit) productExponent++;
    else wideLeftShift(&productHi, &productLo, 1);
    
    // If we have overflowed the type, return +/- infinity.
    if (productExponent >= maxExponent) return fromRep(infRep | productSign);
    
    if (productExponent <= 0) {
        // Result is denormal before rounding, the exponent is zero and we
        // need to shift the significand.
        wideRightShiftWithSticky(&productHi, &productLo, 1 - productExponent);
    }
    
    else {
        // Result is normal before rounding; insert the exponent.
        productHi &= significandMask;
        productHi |= (rep_t)productExponent << significandBits;
    }
    
    // Insert the sign of the result:
    productHi |= productSign;
    
    // Final rounding.  The final result may overflow to infinity, or underflow
    // to zero, but those are the correct results in those cases.
    if (productLo > signBit) productHi++;
    if (productLo == signBit) productHi += productHi & 1;
    return fromRep(productHi);
}