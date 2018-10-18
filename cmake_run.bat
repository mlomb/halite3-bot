if not exist "Build" mkdir Build
cd Build && cmake . .. -DCMAKE_CXX_FLAGS=-DDEBUG
pause