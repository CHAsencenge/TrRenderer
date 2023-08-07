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
	unsigned char* data;
	int width;
	int height;
	int bytesPerPixel;
};

class TGAHeader
{
public:
	char idLength;   // ID长度
	char colorMapType;   // 颜色映射类型
	char dataType;   // 图像类型，包括无压缩真彩色、无压缩索引色、无压缩黑白图像、RLE压缩真彩色、RLE压缩索引色和RLE压缩黑白图像
	short colorMapOrigin;   // 颜色映射表的起始索引
	short colorMapLength;   // 颜色映射表的长度
	char bitsPerColorMapItem;   // 颜色映射表中每个颜色项的位数
	short xOrigin;   // 图像X坐标的起始位置
	short yOrigin;   // 图像Y坐标的起始位置
	short width;   // 图像的宽度
	short height;   // 图像的高度
	char bitsPerPixel;   // 每个像素的位数
	char imageDescriptor;   // 图像描述字节(管图像方向)
};

