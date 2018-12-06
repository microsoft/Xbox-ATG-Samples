#ifndef GENERALH_H
#define GENERALH_H

static const float XM_PI = 3.14159265f;

// Pseudo-random hash.
// Code from http://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/.
uint WangHash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

// Xorshift algorithm from George Marsaglia's paper.
uint Random(uint state)
{
    state ^= (state << 13);
    state ^= (state >> 17);
    state ^= (state << 5);
    return state;
}

uint CreateSeed(uint2 dispatchID)
{
    return
        WangHash(
            WangHash(dispatchID.x)
            ^ Random(dispatchID.y)
        );
}

// Random float between [0, 1).
float RandFloat(inout uint seed)
{
    uint rand = seed = Random(seed);
    return rand * 2.3283064365387e-10;
}

#endif