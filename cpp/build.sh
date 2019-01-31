ROOT_DIR=$(cd "$(dirname $0)"; pwd)
BUILD_DIR="${ROOT_DIR}/build/cmake_build"
INSTALL_DIR="${ROOT_DIR}/build/cmake_install"
PROJ_DIR=${ROOT_DIR}

build() {
  mkdir -p "${BUILD_DIR}"
  cd "${BUILD_DIR}"
  rm -rf CMakeFiles
  rm -rf CMakeCache.txt
  rm -rf Makefile cmake_install.cmake
  cmake "${PROJ_DIR}" -DDEBUG=$DEBUG_FLAG -DBUILD_TEST=$BUILD_TEST -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"
  make -j8
  make install
}


help() {
  echo "Usage:"
  echo "  ./build.sh [option1 option2 ...] debug"
  echo "  ./build.sh [option1 option2 ...] release"
  echo "OPTIONS:"
  echo "  test:          Build unit test cases."
  echo "  clean:         Clean temporary building files."
  echo "  help:          Show this message."
}


clean_all() {
  echo "Cleaning temporary building files..."
  rm -rf "${BUILD_DIR}" "${INSTALL_DIR}"
}


DEBUG_FLAG=OFF
BUILD_TEST=OFF

if [ $# -eq 0 ]; then
    help
    exit 0
fi

while [ ! $# -eq 0 ]; do
  case $1 in
    debug)
      DEBUG_FLAG=ON
      show_config
      build
      shift
    ;;
    release)
      DEBUG_FLAG=OFF
      show_config
      build
      shift
    ;;
    test)
      BUILD_TEST=ON
      shift
    ;;
    clean)
      clean_all
      shift
    ;;
    help)
      help
      exit 0
    ;;
    *)
      help
      exit 0
    ;;
  esac
done