// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(HEIGHTS_SAMPLE_H)
#define HEIGHTS_SAMPLE_H

static const uint HeightsOverlap = 2;

int2  T0(int2 input) { return int2(max(input.x-1, HeightMapOrigin.x),								max(input.y-1, HeightMapOrigin.y)); }
int2  T1(int2 input) { return int2(    input.x+0,													max(input.y-1, HeightMapOrigin.y)); }
int2  T2(int2 input) { return int2(    input.x+1,													max(input.y-1, HeightMapOrigin.y)); }
int2  T3(int2 input) { return int2(min(input.x+2, HeightMapOrigin.x+TileDimensionsInVertices-1),	max(input.y-1, HeightMapOrigin.y)); }

int2  T4(int2 input) { return int2(max(input.x-1, HeightMapOrigin.x),								input.y); }
int2  T5(int2 input) { return int2(min(input.x+2, HeightMapOrigin.x+TileDimensionsInVertices-1),	input.y); }

int2  T6(int2 input) { return int2(max(input.x-1, HeightMapOrigin.x),								input.y+1); }
int2  T7(int2 input) { return int2(min(input.x+2, HeightMapOrigin.x+TileDimensionsInVertices-1),	input.y+1); }

int2  T8(int2 input) { return int2(max(input.x-1, HeightMapOrigin.x),								min(input.y+2, HeightMapOrigin.y+TileDimensionsInVertices-1)); }
int2  T9(int2 input) { return int2(    input.x+0,													min(input.y+2, HeightMapOrigin.y+TileDimensionsInVertices-1)); }
int2 T10(int2 input) { return int2(    input.x+1,													min(input.y+2, HeightMapOrigin.y+TileDimensionsInVertices-1)); }
int2 T11(int2 input) { return int2(min(input.x+2, HeightMapOrigin.x+TileDimensionsInVertices-1),	min(input.y+2, HeightMapOrigin.y+TileDimensionsInVertices-1)); }

float EvaluateCubicCurve(float pm0, float p0, float p1, float p2, float t)
{
        //	evaluate a basic catmul rom curve through the given points
    float t2 = t*t;
    float t3 = t2*t;

    float m0 = .5f * (p1 - pm0);		// catmull rom tangent values
    float m1 = .5f * (p2 - p0);
    return
          p0 * (1 - 3.f * t2 + 2.f * t3)
        + p1 * (3.f * t2 - 2.f * t3)
        + m0 * (t - 2.f * t2 + t3)
        + m1 * (-t2 + t3);
}

float CustomSample(float2 UV, int interpolationQuality)
{
        // todo -- consider doing height interpolation in world space (rather than in
        //			0 - 65535 height map space). This may result in more accurate floating
        //			point numbers.

        //	Note that this high quality interpolation is only really useful when we
        //	tessellation to higher levels than the input texture (ie, if the input
        //	texture is 32x32, but we want to tessellate up to 64x64). So maybe we
        //	can disable it for lower levels of tessellation;

    if (interpolationQuality==1) {

            //	Do our own custom bilinear interpolation across the heights texture
            //	Minimum quality for patches actively changing LOD.

        float2 texelCoords = HeightMapOrigin.xy + UV * float(TileDimensionsInVertices-HeightsOverlap);
        int2 minTexelCorner = int2(texelCoords); // round down
        float2 filter = texelCoords - minTexelCorner;

        int A = HeightsTileSet.Load(int4(minTexelCorner + int2(0,0), HeightMapOrigin.z, 0));
        int B = HeightsTileSet.Load(int4(minTexelCorner + int2(1,0), HeightMapOrigin.z, 0));
        int C = HeightsTileSet.Load(int4(minTexelCorner + int2(0,1), HeightMapOrigin.z, 0));
        int D = HeightsTileSet.Load(int4(minTexelCorner + int2(1,1), HeightMapOrigin.z, 0));

        float w0 = (1.0f - filter.x) * (1.0f - filter.y);
        float w1 = (       filter.x) * (1.0f - filter.y);
        float w2 = (1.0f - filter.x) * (       filter.y);
        float w3 = (       filter.x) * (       filter.y);

        return	  float(A) * w0
                + float(B) * w1
                + float(C) * w2
                + float(D) * w3
                ;

    } else if (interpolationQuality==2) {

            //	Do bicubic interpolation, to pick up implied curves between sample points.
            //	We can improve the performance by storing tangents at height map point.

        float2 texelCoords = HeightMapOrigin.xy + UV * float(TileDimensionsInVertices-HeightsOverlap);
        int2 minTexelCorner = int2(texelCoords); // round down
        float2 filter = texelCoords - minTexelCorner;

            //	Let's try this method:
            //		build 4 horizontal catmull-rom curves, and evaluate these at the
            //		UV.x location.
            //		that defines 4 control points
            //		- make a new vertical curve along those control points
            //		evaluate that curve at the UV.y position

        float A   = (float)HeightsTileSet.Load(int4(minTexelCorner + int2(0,0), HeightMapOrigin.z, 0));
        float B   = (float)HeightsTileSet.Load(int4(minTexelCorner + int2(1,0), HeightMapOrigin.z, 0));
        float C   = (float)HeightsTileSet.Load(int4(minTexelCorner + int2(0,1), HeightMapOrigin.z, 0));
        float D   = (float)HeightsTileSet.Load(int4(minTexelCorner + int2(1,1), HeightMapOrigin.z, 0));

        float t0  = (float)HeightsTileSet.Load(int4( T0(minTexelCorner), HeightMapOrigin.z, 0));
        float t1  = (float)HeightsTileSet.Load(int4( T1(minTexelCorner), HeightMapOrigin.z, 0));
        float t2  = (float)HeightsTileSet.Load(int4( T2(minTexelCorner), HeightMapOrigin.z, 0));
        float t3  = (float)HeightsTileSet.Load(int4( T3(minTexelCorner), HeightMapOrigin.z, 0));
        float t4  = (float)HeightsTileSet.Load(int4( T4(minTexelCorner), HeightMapOrigin.z, 0));
        float t5  = (float)HeightsTileSet.Load(int4( T5(minTexelCorner), HeightMapOrigin.z, 0));
        float t6  = (float)HeightsTileSet.Load(int4( T6(minTexelCorner), HeightMapOrigin.z, 0));
        float t7  = (float)HeightsTileSet.Load(int4( T7(minTexelCorner), HeightMapOrigin.z, 0));
        float t8  = (float)HeightsTileSet.Load(int4( T8(minTexelCorner), HeightMapOrigin.z, 0));
        float t9  = (float)HeightsTileSet.Load(int4( T9(minTexelCorner), HeightMapOrigin.z, 0));
        float t10 = (float)HeightsTileSet.Load(int4(T10(minTexelCorner), HeightMapOrigin.z, 0));
        float t11 = (float)HeightsTileSet.Load(int4(T11(minTexelCorner), HeightMapOrigin.z, 0));

        float q0  = EvaluateCubicCurve(t0, t1, t2, t3, filter.x);
        float q1  = EvaluateCubicCurve(t4,  A,  B, t5, filter.x);
        float q2  = EvaluateCubicCurve(t6,  C,  D, t7, filter.x);
        float q3  = EvaluateCubicCurve(t8, t9,t10,t11, filter.x);

        return EvaluateCubicCurve(q0, q1, q2, q3, filter.y);

    } else {

            //	Just do point sampling. This is not really accurate enough when the tessellation is
            //	changing -- points will jump from height to height and create wierd wrinkles.
            //	It should be ok for patches that are fixed at the lowest LOD, however.

        float2 texelCoords = HeightMapOrigin.xy + UV * float(TileDimensionsInVertices-HeightsOverlap);
        int2 minTexelCorner = int2(floor(texelCoords)); // round down
        return (float)HeightsTileSet.Load(int4(minTexelCorner + int2(0,0), HeightMapOrigin.z, 0));

    }
}

#endif