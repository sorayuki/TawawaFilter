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
    std::unique_ptr<CLKernel> clKernConv;
    std::unique_ptr<CLKernel> clKernBlue;

    std::vector<uint8_t> buffer;
public:
	TawawaFilter(PClip child, IScriptEnvironment* env)
		: GenericVideoFilter(child)
	{
		if (!vi.IsYV12())
			env->ThrowError("TawawaFilter: Only YV12 input is supported.");

        //vi.pixel_type = VideoInfo::CS_BGR24;

        cl.reset(EasyCL::createForFirstGpu());
        clKernBlue.reset(cl->buildKernelFromString(
            "kernel void toblue(global uchar4* in, global uchar4* out) { \n"
            "    const int gid = get_global_id(0); \n"
            "    double y = in[gid].x; \n"
            "    float4 yf = (float4)(0.299, 0.587, 0.114, 0); \n"
            "    float4 uf = (float4)(-0.14713, -0.28886, 0.436, 0); \n"
            "    float4 vf = (float4)(0.615, -0.51499, -0.10001, 0); \n"
            "    float4 rgbResult;"
            "    y = y / 255 *200 + 55; \n"
            "    if (y > 255) y = 255; \n"
            "     \n"
            "    int iy = y; \n"
            "    rgbResult.x = iy > 85 ? ((y - 85) / 255 * 340) : 0; \n"
            "    rgbResult.y = iy; \n"
            "    rgbResult.z = iy > 135 ? 255 : iy + 120; \n"
            "    rgbResult.w = 0; \n"
            "    out[gid].x = dot(rgbResult, yf); \n"
            "    out[gid].y = dot(rgbResult, uf); \n"
            "    out[gid].z = dot(rgbResult, vf); \n"
            //"    out[gid].x = rgbResult.x; \n"
            //"    out[gid].y = rgbResult.y; \n"
            //"    out[gid].z = rgbResult.z; \n"
            "}"
            , "toblue", ""));

        buffer.resize(vi.width * vi.height * 4);
	}

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override
	{
		PVideoFrame frame = child->GetFrame(n, env);

        {
            const uint8_t* pSrcY = frame->GetReadPtr(PLANAR_Y);
            int srcYP = frame->GetPitch(PLANAR_Y);
            const uint8_t* pSrcU = frame->GetReadPtr(PLANAR_U);
            int srcYU = frame->GetPitch(PLANAR_U);
            const uint8_t* pSrcV = frame->GetReadPtr(PLANAR_V);
            int srcYV = frame->GetPitch(PLANAR_V);

            uint8_t* pDst = buffer.data();

            for (int ch = 0; ch < vi.height; ++ch)
            {
                const uint8_t* pcSrcY = pSrcY + ch * srcYP;
                const uint8_t* pcSrcU = pSrcU + ch * srcYU / 2;
                const uint8_t* pcSrcV = pSrcV + ch * srcYV / 2;

                uint8_t(*pcDst)[4] = (uint8_t(*)[4]) (pDst + ch * vi.width * 4);
                for (int cw = 0; cw < vi.width; ++cw)
                {
                    pcDst[cw][0] = pcSrcY[cw];
                    //pcDst[cw][1] = pcSrcY[cw];
                    //pcDst[cw][2] = pcSrcY[cw];
                    //pcDst[cw][3] = pcSrcY[cw];
                    pcDst[cw][1] = pcSrcU[cw / 2];
                    pcDst[cw][2] = pcSrcV[cw / 2];
                    pcDst[cw][3] = 0;
                }
            }
        }

        clKernBlue->in(vi.width * vi.height, (const uint32_t*)buffer.data());
        clKernBlue->out(vi.width * vi.height, (uint32_t*)buffer.data());
        clKernBlue->run_1d(vi.width * vi.height, 512);

        PVideoFrame newFrame = env->NewVideoFrame(vi);

#if 1
        uint8_t* prDstY = newFrame->GetWritePtr(PLANAR_Y);
        int rYP = newFrame->GetPitch(PLANAR_Y);
        uint8_t* prDstU = newFrame->GetWritePtr(PLANAR_U);
        int rUP = newFrame->GetPitch(PLANAR_U);
        uint8_t* prDstV = newFrame->GetWritePtr(PLANAR_V);
        int rVP = newFrame->GetPitch(PLANAR_V);

        {
            uint8_t* pDstY = newFrame->GetWritePtr(PLANAR_Y);
            int dstYP = frame->GetPitch(PLANAR_Y);
            uint8_t* pDstU = newFrame->GetWritePtr(PLANAR_U);
            int dstYU = frame->GetPitch(PLANAR_U);
            uint8_t* pDstV = newFrame->GetWritePtr(PLANAR_V);
            int dstYV = frame->GetPitch(PLANAR_V);

            uint8_t* pSrc = buffer.data();

            for (int ch = 0; ch < vi.height; ++ch)
            {
                uint8_t* pcDstY = pDstY + ch * dstYP;
                uint8_t* pcDstU = pDstU + ch * dstYU / 2;
                uint8_t* pcDstV = pDstV + ch * dstYV / 2;

                uint8_t(*pcSrc)[4] = (uint8_t(*)[4]) (pSrc + ch * vi.width * 4);
                for (int cw = 0; cw < vi.width; ++cw)
                {
                    pcDstY[cw] = pcSrc[cw][0];
                    //pcDstU[cw / 2] = pcSrc[cw][1];
                    //pcDstV[cw / 2] = pcSrc[cw][2];
                    pcDstU[cw / 2] = 128;
                    pcDstV[cw / 2] = 128;
                }
            }
        }
#else
        uint8_t* p1 = newFrame->GetWritePtr();
        int pp = newFrame->GetPitch();

        for (int ch = 0; ch < vi.height; ++ch)
        {
            uint8_t* pc = p1 + pp * ch;
            uint8_t* pc2 = buffer.data() + 4 * vi.width * ch;
            for (int cw = 0; cw < vi.width; ++cw)
            {
                pc[cw * 3] = pc2[cw * 4 + 2];
                pc[cw * 3 + 1] = pc2[cw * 4 + 1];
                pc[cw * 3 + 2] = pc2[cw * 4 + 0];
            }
        }
#endif

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

    env->AddFunction("Tawawa", "c", CreateTawawaFilter, 0);
	return "TawawaFilter";
}
