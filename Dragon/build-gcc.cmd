@rem THIS DOESN'T WORK, BECAUSE THE CODE IS NOT STANDARDS COMPLIANT!!!
g++ CParser.cpp Dragon.cpp IcData.cpp IcDump.cpp IcEvaluator.cpp IcGraph.cpp IcObject.cpp IcSupport.cpp Lexer.cpp LrGenerator.cpp LrParser.cpp OptLocalSccp.cpp -o Dragon.exe -O3 -march=corei7-avx -Werror -std=c++0x -static-libstdc++ -Xlinker --subsystem -Xlinker console