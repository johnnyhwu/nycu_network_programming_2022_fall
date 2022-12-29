#include "src/neter.h"

int main(int argc, char** argv)
{
    string service = argv[1];
    NETer neter = NETer();
    neter.multi_proc_server(service);
    return 0;
}