/*
Copyright (c) 2016, sorayuki
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of this program nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <windows.h>
#include "Avisynth.h"
#include <vector>

#include <EasyCL.h>
#include <memory>

using namespace easycl;

class TawawaFilter : public GenericVideoFilter
{
    std::unique_ptr<EasyCL> cl;
    std::unique_ptr<CLKernel> clKernBlue;

public:
	TawawaFilter(PClip child, IScriptEnvironment* env)
		: GenericVideoFilter(child)
	{
		if (!vi.IsYV12())
			env->ThrowError("TawawaFilter: Only YV12 input is supported.");

        cl.reset(EasyCL::createForFirstGpu());
        clKernBlue.reset(cl->buildKernelFromString(
            "kernel void toblue(int width, int height, "
            "    global uchar* inY, int inYRS, global uchar* inU, int inURS, global uchar* inV, inVRS, \n"
            "    global uchar* outY, int outYRS, global uchar* outU, int outURS, global uchar* outV, int outVRS) \n"
            "{ \n"
            "    const int gid = get_global_id(0); \n"
            "    const int ch = (int)(gid / width); \n"
            "    const int cw = gid - width * ch; \n"
            "    const int ch_half = (ch - (ch % 2)) / 2; \n"
            "    const int cw_half = (cw - (cw % 2)) / 2; \n"
            "    const int idx = ch * inYRS + cw; \n"
            "    const int outYidx = ch * outYRS + cw; \n"
            "    const int outUidx = ch_half * outURS + cw_half; \n"
            "    const int outVidx = ch_half * outVRS + cw_half; \n"
            "    \n"
            "    double y = inY[idx]; \n"
            "    float4 yf = (float4)(0.299, 0.587, 0.114, 0); \n"
            "    float4 uf = (float4)(-0.14713, -0.28886, 0.436, 128); \n"
            "    float4 vf = (float4)(0.615, -0.51499, -0.10001, 128); \n"
            "    float4 rgbResult;"
            "    y = y / 255 *200 + 55; \n"
            "    if (y > 255) y = 255; \n"
            "     \n"
            "    int iy = y; \n"
            "    rgbResult.x = iy > 85 ? ((y - 85) / 255 * 340) : 0; \n"
            "    rgbResult.y = iy; \n"
            "    rgbResult.z = iy > 135 ? 255 : iy + 120; \n"
            "    rgbResult.w = 1; \n"
            "    outY[outYidx] = dot(rgbResult, yf); \n"
            "    outU[outUidx] = dot(rgbResult, uf); \n"
            "    outV[outVidx] = dot(rgbResult, vf); \n"
            "     \n"
            "}"
            , "toblue", ""));
	}

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override
	{
		PVideoFrame frame = child->GetFrame(n, env);

        const uint8_t* pSrcY = frame->GetReadPtr(PLANAR_Y);
        int srcYP = frame->GetRowSize(PLANAR_Y);
        const uint8_t* pSrcU = frame->GetReadPtr(PLANAR_U);
        int srcUP = frame->GetRowSize(PLANAR_U);
        const uint8_t* pSrcV = frame->GetReadPtr(PLANAR_V);
        int srcVP = frame->GetRowSize(PLANAR_V);

        clKernBlue->in(vi.width);
        clKernBlue->in(vi.height);
        clKernBlue->in(srcYP * vi.height / 4, (const uint32_t*)pSrcY);
        clKernBlue->in(srcYP);
        clKernBlue->in(srcUP * vi.height / 4 / 2, (const uint32_t*)pSrcU);
        clKernBlue->in(srcUP);
        clKernBlue->in(srcVP * vi.height / 4 / 2, (const uint32_t*)pSrcV);
        clKernBlue->in(srcVP);

        PVideoFrame newFrame = env->NewVideoFrame(vi);

        uint8_t* pDstY = newFrame->GetWritePtr(PLANAR_Y);
        int dstYP = frame->GetRowSize(PLANAR_Y);
        uint8_t* pDstU = newFrame->GetWritePtr(PLANAR_U);
        int dstUP = frame->GetRowSize(PLANAR_U);
        uint8_t* pDstV = newFrame->GetWritePtr(PLANAR_V);
        int dstVP = frame->GetRowSize(PLANAR_V);

        clKernBlue->out(dstYP * vi.height / 4, (uint32_t*)pDstY);
        clKernBlue->in(dstYP);
        clKernBlue->out(dstUP * vi.height / 4 / 2, (uint32_t*)pDstU);
        clKernBlue->in(dstUP);
        clKernBlue->out(dstVP * vi.height / 4 / 2, (uint32_t*)pDstV);
        clKernBlue->in(dstVP);

        clKernBlue->run_1d(vi.width * vi.height, cl->getMaxWorkgroupSize());

		return newFrame;
	}
};

AVSValue __cdecl CreateTawawaFilter(AVSValue args, void* user_data, IScriptEnvironment* env)
{
	return new TawawaFilter(args[0].AsClip(), env);
}

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, void* wtf)
{
    if (!EasyCL::isOpenCLAvailable())
        env->ThrowError("%s", "OpenCL is not supported in your system.");

    env->AddFunction("TawawaGPU", "c", CreateTawawaFilter, 0);
	return "TawawaGPUFilter";
}
