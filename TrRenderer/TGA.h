#pragma once
#include <fstream>
#include <iostream>

class TGAColor
{
public:
	unsigned char bgra[4];
	unsigned char bytesPerPixel;
	TGAColor() : bgra(), bytesPerPixel(1)
	{
		for (int i = 0; i < 4; i++)
		{
			bgra[i] = 0;
		}
	}

	TGAColor(unsigned char R, unsigned char G, unsigned char B, unsigned char A = 255) : bgra(), bytesPerPixel(4)
	{
		bgra[0] = B;
		bgra[1] = G;
		bgra[2] = R;
		bgra[3] = A;
	}

	TGAColor(unsigned char gray) : bgra(), bytesPerPixel(1)
	{
		for (int i = 0; i < 4; i++)
		{
			bgra[i] = 0;
		}
		bgra[0] = gray;
	}

	TGAColor(unsigned char* p, unsigned char bpp) : bgra(), bytesPerPixel(bpp)
	{
		for (int i = 0; i < (int)bpp; i++)
		{
			bgra[i] = p[i];
		}
		for (int i = int(bpp); i < 4; i++)
		{
			bgra[i] = 0;
		}
	}
};

// TGA文件格式支持多种压缩方式，包括无压缩、RLE压缩和Delta压缩等。其中，RLE压缩是一种常用的压缩方式，可以有效地减小文件大小
class TGAImage
{
public:
	TGAImage(int w, int h, int bpp = 0) 
	{
		width = w;
		height = h;
		// bytesPerPixel实际会在ReadRLEData时计算
		bytesPerPixel = bpp;
		data = nullptr;
	}

	// 读TGA文件
	bool ReadTGAFile(const char* filename);
	// 竖直翻转
	void FilpVertically();
	void FilpHorizontally();

	bool ReadRLEData(std::ifstream& in);

	bool UnloadRLEData(std::ofstream &out);

	bool WriteTGAFile(const char* filename, bool vflip, bool rle = true);

	TGAColor Get(int x, int y);
	bool Set(int x, int y, TGAColor& c);
	bool Set(int x, int y, const TGAColor& c);

	int Width() { return width; }
	int Height() { return height; }

public:
	enum Format
	{
		GRAYSCALE = 1,
		RGB = 3,
		RGBA = 4
	};

protected:
	// unsigned char指针通常用于处理二进制数据，例如读取和修改图像数据、音频数据等
	// 处理二进制数据需要使用unsigned char指针来确保数据的正确性
	unsigned char* data = nullptr;
	int width = 0;
	int height = 0;
	uint8_t bytesPerPixel = 0;
};

// 将当前的对齐方式压栈，并将对齐方式设置为1字节对齐。也就是说，结构体的成员将按照一个字节的边界进行对齐，不会有任何填充字节
#pragma pack(push,1)
struct TGAHeader
{

	uint8_t idLength = 0;   // ID长度
	uint8_t colorMapType = 0;   // 颜色映射类型
	uint8_t dataType = 0;   // 图像类型，包括无压缩真彩色、无压缩索引色、无压缩黑白图像、RLE压缩真彩色、RLE压缩索引色和RLE压缩黑白图像
	uint16_t colorMapOrigin = 0;   // 颜色映射表的起始索引
	uint16_t colorMapLength = 0;   // 颜色映射表的长度
	uint8_t bitsPerColorMapItem = 0;   // 颜色映射表中每个颜色项的位数
	uint16_t xOrigin = 0;   // 图像X坐标的起始位置
	uint16_t yOrigin = 0;   // 图像Y坐标的起始位置
	uint16_t width = 0;   // 图像的宽度
	uint16_t height = 0;   // 图像的高度
	uint8_t bitsPerPixel = 0;   // 每个像素的位数
	uint8_t imageDescriptor = 0;   // 图像描述字节(管图像方向)
};
#pragma pack(pop)

