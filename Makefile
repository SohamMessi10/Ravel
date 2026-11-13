CXXFLAGS = -std=c++20 -O2 -Wall -Wextra -pedantic
.PHONY: test demo clean
ravel: src/main.cpp src/engine.hpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o ravel
ravel-test: tests/test.cpp src/engine.hpp
	$(CXX) $(CXXFLAGS) tests/test.cpp -o ravel-test
test: ravel-test
	./ravel-test
demo: ravel
	./ravel fixtures/session.txt
clean:
	$(RM) ravel ravel-test
