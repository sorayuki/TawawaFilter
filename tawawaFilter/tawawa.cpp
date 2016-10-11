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

class TawawaFilter : public GenericVideoFilter
{
public:
	TawawaFilter(PClip child, IScriptEnvironment* env)
		: GenericVideoFilter(child)
	{
		if (!vi.IsRGB24())
			env->ThrowError("TawawaFilter: Only RGB24 input is supported.");
	}

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override
	{
		PVideoFrame frame = child->GetFrame(n, env);
		PVideoFrame newFrame = env->NewVideoFrame(vi);

		const unsigned char* pSrc = frame->GetReadPtr();
		unsigned char* pDst = newFrame->GetWritePtr();

		int srcPitch = frame->GetPitch();
		int dstPitch = newFrame->GetPitch();



		for (int ch = 0; ch < vi.height; ++ch)
		{
			const unsigned char* pcSrc = pSrc + srcPitch * ch;
			unsigned char* pcDst = pDst + dstPitch * ch;

			for (int cw = 0; cw < vi.width; ++cw)
			{
				double y = pcSrc[2] * 0.3 + pcSrc[1] * 0.59 + pcSrc[0] * 0.11;
				y = y / 255 * 200 + 55;
				if (y > 255) y = 255;

				int colorR, colorG;

				if (y < 70)
					colorR = 0;
				else if (y < 85)
					colorR = (y - 70) * 4.0;
				else
					colorR = (y - 85) * 1.0 / 170 * 195 + 60;
				
				pcDst[2] = colorR;

				if (y < 70)
					colorG = 90;
				else if (y < 85)
					colorG = (y - 70) * 2.6 + 90;
				else
					colorG = (y - 85) * 1.0 / 170 * 126 + 129;

				pcDst[1] = colorG;

				pcDst[0] = 255;

				pcSrc += 3;
				pcDst += 3;
			}
		}

		return newFrame;
	}
};

AVSValue __cdecl CreateTawawaFilter(AVSValue args, void* user_data, IScriptEnvironment* env)
{
	return new TawawaFilter(args[0].AsClip(), env);
}

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, void* wtf)
{
	env->AddFunction("Tawawa", "c", CreateTawawaFilter, 0);
	return "TawawaFilter";
}
