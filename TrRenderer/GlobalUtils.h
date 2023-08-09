#pragma once
#include <bitset>
#include <iostream>

namespace TrDebug
{
    template <typename T, typename CastType = int>
    void PrintArray(T& inArray, bool needCast = false)
    {
        for (int i = 0; i < sizeof(inArray)/ sizeof(inArray[0]); i++)
        {
            if (needCast)
            {
                std::cout << static_cast<CastType>(inArray[i]) << " ";   
            }
            else
            {
                std::cout << inArray[i] << " "; 
            }
        }
        std::cout << std::endl;
    }

    template <typename T>
    void PrintBits(const char* prefix, T& inStruct)
    {
        std::bitset<sizeof(inStruct)*8> bits(inStruct);
        std::cout << prefix << bits << std::endl;
    }
    
}
