#include "TGA.h"
#include <fstream>
#include <iostream>

// 使用ifstream读文件
bool TGAImage::ReadTGAFile(const char* filename)
{
	if (data)
		delete[] data;
	std::ifstream in;
	// 要以二进制形式打开
	in.open(filename, std::ios::binary);
	// 是否打开成功
	if (!in.is_open())
	{
		std::cerr << "ReadTGAFile: can't open file " << filename << "\n";
		in.close();
		return false;
	}
	TGAHeader header;
	in.read((char*)&header, sizeof(header));
	// 是否读取成功
	if (!in.good())
	{
		std::cerr << "ReadTGAFile: can't read " << filename << "header\n";
		in.close();
		return false;
	}
	width = header.width;
	height = header.height;
	bytesPerPixel = header.bitsPerPixel >> 3;
	if (width <= 0 || height <= 0 || (bytesPerPixel != GRAYSCALE && bytesPerPixel != RGB && bytesPerPixel != RGBA))
	{
		std::cerr << "ReadTGAFile: bad bitsPerPixel or width/height value of" << filename << "\n";
		in.close();
		return false;
	}
	
	// 是否读到末尾
	// in.eof();
	// 读取失败
	// in.fail();
}

void TGAImage::FilpVertically()
{
}
