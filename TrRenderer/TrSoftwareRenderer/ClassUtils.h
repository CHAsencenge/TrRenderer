#pragma once
#include "Maths.h"

class TrUtils
{
public:
    template<int nrow, int ncol>
    static void PrintMat(Mat<nrow, ncol> mat, const char* prefix = "")
    {
        if(strcmp(prefix, "") != 0)
        {
            std::cout << prefix << std::endl;
        }
        for (int i = 0; i < nrow; i++)
        {
            TrDebug::PrintArray(mat.rows[i].e, false);
        }
    }
};
