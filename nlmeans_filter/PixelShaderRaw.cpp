#include "stdafx.h"
#include <d3dx9.h>
#include "PixelShaderRaw.h"

using namespace std;

const char* PixelShaderRaw::PIXEL_SHADER = 
"static const int spaceSearchRadius = SPACE_SEARCH_RADIUS;\n"
"static const int timeSearchRadius = TIME_SEARCH_RADIUS;\n"
"static const int timeSearchDiameter = TIME_SEARCH_RADIUS * 2 + 1;\n"
"static const float2 DELTA[9] = {\n"
"	float2(-1, -1),\n"
"	float2( 0, -1),\n"
"	float2( 1, -1),\n"
"	float2(-1,  0),\n"
"	float2( 0,  0),\n"
"	float2( 1,  0),\n"
"	float2(-1,  1),\n"
"	float2( 0,  1),\n"
"	float2( 1,  1)\n"
"};\n"
"sampler2D textureSampler[timeSearchDiameter];\n"
"float2 textureSizeInverse;\n"
"float H2;\n"
"\n"
"float4 process(float2 pos : VPOS) : COLOR0\n"
"{\n"
"	//あらかじめ摂動を入れてサンプルポイントが画素の中央を指すようにする\n"
"	pos += 0.25;\n"
"	pos *= textureSizeInverse;\n"
"\n"
"	const float2 delta[9] = {\n"
"		DELTA[0] * textureSizeInverse,\n"
"		DELTA[1] * textureSizeInverse,\n"
"		DELTA[2] * textureSizeInverse,\n"
"		DELTA[3] * textureSizeInverse,\n"
"		DELTA[4] * textureSizeInverse,\n"
"		DELTA[5] * textureSizeInverse,\n"
"		DELTA[6] * textureSizeInverse,\n"
"		DELTA[7] * textureSizeInverse,\n"
"		DELTA[8] * textureSizeInverse,\n"
"	};\n"
"\n"
"	const float3 currentPixels[9] = {\n"
"		tex2D(textureSampler[timeSearchRadius], pos + delta[0]).xyz,\n"
"		tex2D(textureSampler[timeSearchRadius], pos + delta[1]).xyz,\n"
"		tex2D(textureSampler[timeSearchRadius], pos + delta[2]).xyz,\n"
"		tex2D(textureSampler[timeSearchRadius], pos + delta[3]).xyz,\n"
"		tex2D(textureSampler[timeSearchRadius], pos + delta[4]).xyz,\n"
"		tex2D(textureSampler[timeSearchRadius], pos + delta[5]).xyz,\n"
"		tex2D(textureSampler[timeSearchRadius], pos + delta[6]).xyz,\n"
"		tex2D(textureSampler[timeSearchRadius], pos + delta[7]).xyz,\n"
"		tex2D(textureSampler[timeSearchRadius], pos + delta[8]).xyz,\n"
"	};\n"
"\n"
"	float3 sum = 0;\n"
"	float3 value = 0;\n"
"	for (int dx = -spaceSearchRadius; dx <= spaceSearchRadius; ++dx) {\n"
"		for (int dy = -spaceSearchRadius; dy <= spaceSearchRadius; ++dy) {\n"
"			for (int dt = 0; dt <= timeSearchRadius * 2; ++dt){\n"
"				const float2 targetPos = pos + float2(dx, dy) * textureSizeInverse;\n"
"\n"
"				float3 sum2 = 0;\n"
"				float3 targetCenter;\n"
"				float3 diff;\n"
"				diff = currentPixels[0] - tex2D(textureSampler[dt], targetPos + delta[0]);\n"
"				sum2 += diff * diff;\n"
"				diff = currentPixels[1] - tex2D(textureSampler[dt], targetPos + delta[1]);\n"
"				sum2 += diff * diff;\n"
"				diff = currentPixels[2] - tex2D(textureSampler[dt], targetPos + delta[2]);\n"
"				sum2 += diff * diff;\n"
"				diff = currentPixels[3] - tex2D(textureSampler[dt], targetPos + delta[3]);\n"
"				sum2 += diff * diff;\n"
"				diff = currentPixels[4] - (targetCenter = tex2D(textureSampler[dt], targetPos + delta[4]));\n"
"				sum2 += diff * diff;\n"
"				diff = currentPixels[5] - tex2D(textureSampler[dt], targetPos + delta[5]);\n"
"				sum2 += diff * diff;\n"
"				diff = currentPixels[6] - tex2D(textureSampler[dt], targetPos + delta[6]);\n"
"				sum2 += diff * diff;\n"
"				diff = currentPixels[7] - tex2D(textureSampler[dt], targetPos + delta[7]);\n"
"				sum2 += diff * diff;\n"
"				diff = currentPixels[8] - tex2D(textureSampler[dt], targetPos + delta[8]);\n"
"				sum2 += diff * diff;\n"
"\n"
"				const float3 w = exp(-sqrt(sum2) * H2);\n"
"				sum += w;\n"
"				value += targetCenter * w;\n"
"			}\n"
"		}\n"
"	}\n"
"\n"
"	value /= sum;\n"
"\n"
"	return float4(value, 0);\n"
"};\n";

PixelShaderRaw::PixelShaderRaw(const CComPtr<IDirect3DDevice9>& device)
{
	this->device = device;
}

PixelShaderRaw::~PixelShaderRaw()
{
}

CComPtr<IDirect3DPixelShader9> PixelShaderRaw::create(int spaceSearchRadius, int timeSearchRadius)
{
	//シェーダプログラムの中のマクロ定義
	char spaceSearchRadiusBuffer[16];
	char timeSearchRadiusBuffer[16];
	sprintf(spaceSearchRadiusBuffer, "%d", spaceSearchRadius);
	sprintf(timeSearchRadiusBuffer, "%d", timeSearchRadius);

	D3DXMACRO macros[3] = {0};
	macros[0].Name = "SPACE_SEARCH_RADIUS";
	macros[0].Definition = spaceSearchRadiusBuffer;
	macros[1].Name = "TIME_SEARCH_RADIUS";
	macros[1].Definition = timeSearchRadiusBuffer;

	//コンパイル
	CComPtr<ID3DXBuffer> buffer;
	CComPtr<ID3DXBuffer> errorMessage;

	if (FAILED(D3DXCompileShader(PIXEL_SHADER, strlen(PIXEL_SHADER), macros, NULL, "process", "ps_3_0", D3DXSHADER_PREFER_FLOW_CONTROL, &buffer, &errorMessage, NULL))){
		//outputLogMessage("ピクセルシェーダのコンパイルに失敗しました");
		//outputLogMessage((char*)errorMessage->GetBufferPointer());
		return FALSE;
	}

	//ピクセルシェーダの作成
	CComPtr<IDirect3DPixelShader9> pixelShader;
	if (FAILED(device->CreatePixelShader((DWORD*)buffer->GetBufferPointer(), &pixelShader))){
		//outputLogMessage("ピクセルシェーダの作成に失敗しました");
		return FALSE;
	}

	return pixelShader;
}
