#include <unistd.h>
#include <vector>
#include <string>
#include "src/userio.h"
#include "src/cmder.h"

int main()
{  
    // initialize
    UserIO user_io = UserIO(true);
    vector<string> cmd_vec;
    CMDer cmder = CMDer();
    
    while(true) {
        // display prompt
        user_io.output("% ");
        
        // wait for user input
        cmd_vec = user_io.input();

        // if user does not input anything
        if(cmd_vec.size() == 0) {
            continue;
        }

        // parse and execute user command
        if(cmder.exec(cmd_vec) == 0) {
            break;
        }
    }

    return 0;
}
