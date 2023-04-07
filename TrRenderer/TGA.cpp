#include "TGA.h"

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
	// istream& read(char* s, streamsize n);
	// s是指向字符数组的指针，用于存储读取的数据；n是要读取的字节数
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

	unsigned long nBytes = width * height * bytesPerPixel;
	// 2：无压缩的真彩色图像
	// 3：无压缩的索引彩色图像
	// 10：RLE压缩的真彩色图像(Run-Length Encoding)
	// 11：RLE压缩的索引彩色图像
	// 
	// 索引彩色图像
	// 使用一个颜色索引表（也称为调色板）来存储图像中使用的所有颜色
	// 索引彩色图像中，每个像素点只需要一个字节来存储颜色索引，而不是直接存储RGB颜色值
	// 
	// RLE
	// RLE压缩的基本思想是将连续的重复数据用一个计数器和一个值来表示，从而减少数据的存储空间
	data = new unsigned char[nBytes];
	if (header.dataType == 2 || header.dataType == 3)
	{
		in.read((char*)data, nBytes);
		if (!in.good())
		{
			in.close();
			std::cerr << "ReadTGAFile: an error occured while reading the data\n";
			return false;
		}
	}
	else if (header.dataType == 10 || header.dataType == 11)
	{
		if (!ReadRLEData(in))
		{

		}
	}

	// 是否读到末尾
	// in.eof();
	// 读取失败
	// in.fail();
}

void TGAImage::FilpVertically()
{
}

bool TGAImage::ReadRLEData(std::ifstream& in)
{
	unsigned long pixelCount = width * height;
	unsigned long currentPixel = 0;
	unsigned long currentByte = 0;

	// 此时TGAImage的数据成员bytesPerPixel应当已经通过读取header被赋值
	while (currentPixel < pixelCount)
	{

		currentPixel++;
	}

	return false;
}
