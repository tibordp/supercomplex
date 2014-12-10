#include "stdio.h"
#include <stdexcept>
int test(char* regex, char* regex1);
int main()
{
    try {
        test("()*", "");
        return 0;
    }
    catch (const std::runtime_error& e)
    {
        printf(e.what());
        return 1;
    }
}