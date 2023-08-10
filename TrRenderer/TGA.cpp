#include "pch.h"
#include "TGA.h"

#include <bitset>
#include <string>

#include "GlobalUtils.h"

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
	in.read(reinterpret_cast<char*>(&header), sizeof(header));
	
	std::cout << "TGAImage::ReadTGAFile: " << std::endl;
	TrDebug::PrintBits("header.idLength ", header.idLength);
	TrDebug::PrintBits("header.colorMapType ", header.colorMapType);
	TrDebug::PrintBits("header.dataType ", header.dataType);
	TrDebug::PrintBits("header.colorMapOrigin ", header.colorMapOrigin);
	TrDebug::PrintBits("header.colorMapLength ", header.colorMapLength);
	TrDebug::PrintBits("header.bitsPerColorMapItem ", header.bitsPerColorMapItem);
	TrDebug::PrintBits("header.xOrigin ", header.xOrigin);
	TrDebug::PrintBits("header.yOrigin ", header.yOrigin);
	TrDebug::PrintBits("header.width ", header.width);
	TrDebug::PrintBits("header.height ", header.height);
	TrDebug::PrintBits("header.bitsPerPixel ", header.bitsPerPixel);
	TrDebug::PrintBits("header.imageDescriptor ", header.imageDescriptor);
	
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
	
	std::bitset<8> bytesPerPixelBits(bytesPerPixel);
	std::cout << "TGAImage::ReadTGAFile bytesPerPixel " << bytesPerPixelBits << std::endl;
	
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
			std::cerr << "ReadTGAFile: an error occured while reading the data 0\n";
			return false;
		}
	}
	else if (header.dataType == 10 || header.dataType == 11)
	{
		std::cout << "TGAImage::ReadRLEData" << filename << std::endl;
		if (!ReadRLEData(in))
		{
			in.close();
			std::cerr << "ReadTGAFile: an error occured while reading the data 1\n";
		}
	}
	else
	{
		in.close();
		std::cerr << "ReadTGAFile: unknown file format \n";
	}

	if (header.imageDescriptor & 0x20)
	{
		FilpVertically();
	}
	if (header.imageDescriptor & 0x10)
	{
		FilpHorizontally();
	}
	in.close();
	return true;

	// 是否读到末尾
	// in.eof();
	// 读取失败
	// in.fail();
}

void TGAImage::FilpVertically()
{

}

void TGAImage::FilpHorizontally()
{

}

// 一个包含两个数据块的RLE数据示例
// Header1 Data1 Header2 Data2
// 10000010 01100100 00000110 01100001 01100010 01100011
bool TGAImage::ReadRLEData(std::ifstream& in)
{
	unsigned long pixelCount = width * height;
	unsigned long currentPixel = 0;
	unsigned long currentByte = 0;
	TGAColor colorBuffer;

	// 此时TGAImage的数据成员bytesPerPixel应当已经通过读取header被赋值
	while (currentPixel < pixelCount)
	{
		unsigned char chunkHeader = 0;
		chunkHeader = in.get();
		if (!in.good())
		{
			std::cerr << "TGAImage::ReadRLEData: an error occured while reading the data\n";
			return false;
		}
		// if highest bit is 0, data block contains multiple consecutive non-repeat data
		if (chunkHeader < 128) 
		{
			chunkHeader++; // why? read chunk+1 times?
			for (int n = 0; n < chunkHeader; n++)
			{
				// non-repeated data blocks
				in.read(reinterpret_cast<char*>(colorBuffer.bgra), bytesPerPixel);
				// std::cout << "chunk data non-repeated: "<< reinterpret_cast<char*>(colorBuffer.bgra) << std::endl;
				if (!in.good())
				{
					std::cerr << "TGAImage::ReadRLEData: an error occured while reading the data 1\n";
					return false;
				}
				// a channel per byte
				for (int i = 0; i < bytesPerPixel; i++)
				{
					data[currentByte++] = colorBuffer.bgra[i];
				}
				currentPixel++;
			}
		}
		// if highest bit is 1, repeated data
		else 
		{
			// why subtract 127 rather than 128? one data block uses 10000000 or 10000001? 
			chunkHeader -= 127;
			in.read(reinterpret_cast<char*>(colorBuffer.bgra), bytesPerPixel); // repeated data block, read once
			
			TrDebug::PrintArray<unsigned char[4], int>(colorBuffer.bgra, true);
			
			if (!in.good())
			{
				std::cerr << "TGAImage::ReadRLEData: an error occured while reading the data 2\n";
				return false;
			}
			for (int n = 0; n < chunkHeader; n++)
			{
				for (int i = 0; i < bytesPerPixel; i++)
				{
					data[currentByte++] = colorBuffer.bgra[i];
				}
				currentPixel++;
			}
		}
		
	}

	return true;
}

// out stream data length: width * height * bytesPerPixel
bool TGAImage::UnloadRLEData(std::ofstream& out)
{
	
	unsigned long pixelCount = width * height;
	unsigned long currentPixel = 0;

	uint8_t maxChunkLength = 128; // can not express more in one byte
	
	while (currentPixel < pixelCount)
	{
		unsigned long chunkStart = currentPixel * bytesPerPixel;
		unsigned long currentByte = currentPixel * bytesPerPixel;
		uint8_t runLength = 1;
		unsigned char chunkHeader = 0;
		// non-repeated
		bool bRaw = true;

		while(currentPixel + runLength < pixelCount && runLength < maxChunkLength)
		{
			bool bEqual = true;
			// if this data color is equal to the next data color
			for(int batchByte = 0; bEqual && batchByte < bytesPerPixel; batchByte++)
			{
				// dataB1 dataG1 dataR1 dataA1 dataB2 dataG2 dataR2 dataA2
				// check equal for each color channel
				bEqual = data[currentByte + batchByte] == data[currentByte + batchByte + bytesPerPixel];
			}
			currentByte += bytesPerPixel;
			if (runLength == 1)
			{
				bRaw = !bEqual;
			}
			// break for non-repeated data
			if (bRaw && bEqual)
			{
				runLength--;
				break;
			}
			// break for repeated data
			if(!bRaw && !bEqual)
			{
				break;
			}
			
			runLength++;
		}
		currentPixel += runLength;
		out.put(bRaw? runLength - 1 : runLength + 127);
		if (!out.good()) {
			std::cerr << "can't dump the tga file : out stream put fail\n";
			return false;
		}
		out.write(reinterpret_cast<const char *>(data + chunkStart), (bRaw ? runLength * bytesPerPixel : bytesPerPixel));
		if (!out.good()) {
			std::cerr << "can't dump the tga file : out stream write fail\n";
			return false;
		}
	}
	return true;
}

bool TGAImage::WriteTGAFile(const char* filename, bool vflip, bool rle)
{
	constexpr std::uint8_t developer_area_ref[4] = {0, 0, 0, 0};
	constexpr std::uint8_t extension_area_ref[4] = {0, 0, 0, 0};
	constexpr std::uint8_t footer[18] = {'T','R','U','E','V','I','S','I','O','N','-','X','F','I','L','E','.','\0'};
	
	std::ofstream out;
	out.open(filename, std::ios::binary);
	if (!out.is_open()) {
		std::cerr << "can't open file " << filename << "\n";
		return false;
	}
	TGAHeader header = {};
	
	TrDebug::PrintBits("bytesPerPixel ", bytesPerPixel);
	
	header.bitsPerPixel = bytesPerPixel << 3;
	header.width  = width;
	header.height = height;
	header.dataType = bytesPerPixel==GRAYSCALE?(rle?11:3):(rle?10:2);
	header.imageDescriptor = vflip ? 0x00 : 0x20; // top-left or bottom-left origin
	out.write(reinterpret_cast<const char *>(&header), sizeof(header));

	std::cout << "TGAImage::WriteTGAFile: " << std::endl;
	TrDebug::PrintBits("header.idLength ", header.idLength);
	TrDebug::PrintBits("header.colorMapType ", header.colorMapType);
	TrDebug::PrintBits("header.dataType ", header.dataType);
	TrDebug::PrintBits("header.colorMapOrigin ", header.colorMapOrigin);
	TrDebug::PrintBits("header.colorMapLength ", header.colorMapLength);
	TrDebug::PrintBits("header.bitsPerColorMapItem ", header.bitsPerColorMapItem);
	TrDebug::PrintBits("header.xOrigin ", header.xOrigin);
	TrDebug::PrintBits("header.yOrigin ", header.yOrigin);
	TrDebug::PrintBits("header.width ", header.width);
	TrDebug::PrintBits("header.height ", header.height);
	TrDebug::PrintBits("header.bitsPerPixel ", header.bitsPerPixel);
	TrDebug::PrintBits("header.imageDescriptor ", header.imageDescriptor);
	
	if (!out.good()) {
		std::cerr << "can't dump the tga file\n";
		return false;
	}
	
	if (!rle) {
		out.write(reinterpret_cast<const char *>(data), width*height*bytesPerPixel);
		if (!out.good())
		{
			std::cerr << "can't unload raw data\n";
			return false;
		}
	}
	else if (!UnloadRLEData(out))
	{
		std::cerr << "can't unload rle data\n";
		return false;
	}
	out.write(reinterpret_cast<const char *>(developer_area_ref), sizeof(developer_area_ref));
	if (!out.good()) {
		std::cerr << "can't dump the tga file developer_area_ref\n";
		return false;
	}
	out.write(reinterpret_cast<const char *>(extension_area_ref), sizeof(extension_area_ref));
	if (!out.good()) {
		std::cerr << "can't dump the tga file extension_area_ref\n";
		return false;
	}
	out.write(reinterpret_cast<const char *>(footer), sizeof(footer));
	if (!out.good()) {
		std::cerr << "can't dump the tga file footer\n";
		return false;
	}

	out.close();

	/*std::ifstream infile(filename, std::ios::binary);
	std::string line;
	while (getline(infile, line))
	{
		std::cout << line << std::endl;
	}
	infile.close();*/
	
	std::cout << "TGAImage::WriteTGAFile end\n";
	return true;
}

TGAColor TGAImage::Get(int x, int y)
{
	if (!data || x < 0 || y < 0 || x >= width || y >= height)
	{
		return TGAColor();
	}
	return TGAColor(data + (x + width * y) * bytesPerPixel, bytesPerPixel);
}

bool TGAImage::Set(int x, int y, TGAColor& c)
{
	if (!data || x < 0 || y < 0 || x >= width || y >= height)
	{
		return false;
	}
	memcpy(data + (x + y * width) * bytesPerPixel, c.bgra, bytesPerPixel);
	return true;
}

bool TGAImage::Set(int x, int y, const TGAColor& c)
{
	if (!data || x < 0 || y < 0 || x >= width || y >= height)
	{
		return false;
	}
	memcpy(data + (x + y * width) * bytesPerPixel, c.bgra, bytesPerPixel);
	return true;
}
