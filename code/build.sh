CODE_PATH="$(dirname "$0")"
CTIME_EXEC="$CODE_PATH/../../ctime/ctime"
CTIME_TIMING_FILE="$CODE_PATH/../build/compile_ray.ctm"

if [ ! -f "$CTIME_EXEC" ]; then
	cc -O2 -Wno-unused-result "$CODE_PATH/../../ctime/ctime.c" -o "$CTIME_EXEC"
fi

$CTIME_EXEC -begin "$CTIME_TIMING_FILE"

CommonFlags="-g -std=c++11 -Werror -Wall -Wextra -Wcast-align -Wmissing-noreturn -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self -Wmissing-include-dirs -Wno-old-style-cast -Woverloaded-virtual -Wredundant-decls -Wsign-promo -Wstrict-overflow=5 -Wundef -Wno-unused -Wno-variadic-macros -Wno-parentheses -fdiagnostics-show-option -Wno-write-strings -Wno-absolute-value -Wno-cast-align -Wno-unused-parameter -lm"

if [ -n "$(command -v clang++)" ]
then
	CXX=clang++
	CommonFlags+=" -Wno-missing-braces -Wno-null-dereference -Wno-self-assign"
else
	CXX=c++
	CommonFlags+=" -Wno-unused-but-set-variable"
fi

CommonLinkerFlags="-l SDL2"

mkdir -p "$CODE_PATH/../build"
pushd "$CODE_PATH/../build"

$CXX $CommonFlags -O3 ../code/ray.cpp $CommonLinkerFlags -o ray-x86_64

popd

$CTIME_EXEC -end "$CTIME_TIMING_FILE"
