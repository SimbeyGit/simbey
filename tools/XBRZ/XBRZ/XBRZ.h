#pragma once

// Ported from the C# project:
// https://github.com/Kianakiferi/My-xBRZ-Image-Convertor/tree/master/xBRZNet

#define	XBRZ_SCALE_2X			0
#define	XBRZ_SCALE_3X			1
#define	XBRZ_SCALE_4X			2
#define	XBRZ_SCALE_5X			3

struct SCALER_CONFIG
{
	double LuminanceWeight;
	double EqualColorTolerance;
	double DominantDirectionThreshold;
	double SteepDirectionThreshold;
};

// XBRZScaleImage()
// Parameters:
//   pConfig (optional) - The configuration for upscaling the image.  If NULL, the default configuration is used.
//   pcnImage (required) - Array of INT, with each element being an RGB pixel.
//   pcszImage (required) - SIZE (pixels) of pcnImage.
//   nScaleSize (required) - Pass one of the XBRZ_SCALE_#X constants.
//   ppnOutput (required) - Receives the upscaled image, use XBRZFreeImage() to free this memory.
//   pszOutput (required) - Receives the size of the upscaled image, in pixels.
HRESULT XBRZScaleImage (__in_opt const SCALER_CONFIG* pcConfig, const INT* pcnImage, const SIZE* pcszImage, INT nScaleSize, __deref_out INT** ppnOutput, __out SIZE* pszOutput);

// XBRZFreeImage()
// Parameters:
//   pnImage (optional) - Frees memory allocated by XBRZScaleImage(), does nothing if pnImage is NULL.
VOID XBRZFreeImage (__in_opt INT* pnImage);

// XBRZGetDefaults()
// Parameters:
//   pConfig (required) - Retrieves the default configuration.
VOID XBRZGetDefaults (__out SCALER_CONFIG* pConfig);
