#!/bin/bash

# Define custom installation directory
TMP_DIR="/tmp/svocab_install_libraries"

install_if_missing_linux_package() {
    local pkg=$1
    if dpkg -s "$pkg" >/dev/null 2>&1; then
        echo "$pkg is already installed."
    else
        echo "$pkg is not installed. Installing..."
        sudo apt install -y "$pkg"
    fi
}

# Function to install libraries on Linux
install_linux() {
    echo "Installing libraries on Linux..."

    # List of packages to check
    packages=(
        git
        gcc
        g++
        cmake
        libjsoncpp-dev
        uuid-dev
        zlib1g-dev
        openssl
        libssl-dev
        postgresql-all
        libhiredis-dev
        gumbo-parser
        libicu-dev
        libpoppler-cpp-dev
        libtool
        libcurl4-openssl-dev
    )

    # Update package list
    sudo apt update

    # Iterate over the list of packages
    for pkg in "${packages[@]}"; do
        install_if_missing_linux_package "$pkg"
    done

    rm -rf $TMP_DIR
    mkdir $TMP_DIR

    if [ "$REINSTALL" = true ] || [ ! -d "/usr/local/include/drogon" ]; then
        cd $TMP_DIR
        git clone https://github.com/drogonframework/drogon.git
        cd drogon
        git submodule update --init
        mkdir build
        cd build
        cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF ..
        sudo make install
    fi

    if [ "$REINSTALL" = true ] || [ ! -f "/usr/local/lib/libgumbo.a" ]; then
        cd $TMP_DIR
        git clone https://github.com/google/gumbo-parser.git
        cd gumbo-parser
        ./autogen.sh
        ./configure
        sudo make install
    fi

    if [ "$REINSTALL" = true ] || [ ! -d "/usr/local/include/jwt-cpp" ]; then
        cd $TMP_DIR
        git clone https://github.com/Thalhammer/jwt-cpp.git
        cd jwt-cpp/include
        sudo cp -rf jwt-cpp /usr/local/include
        sudo cp -rf picojson /usr/local/include
    fi

    rm -rf $TMP_DIR
}

# Function to install libraries on macOS
install_macos() {
    CXX_PATH=/opt/homebrew/Cellar/gcc/14.1.0_2/bin/g++-14
    CC_PATH=/opt/homebrew/Cellar/gcc/14.1.0_2/bin/gcc-14
    REINSTALL=false

    # Check if Homebrew is installed
    if ! command -v brew &> /dev/null; then
        echo "Homebrew not found. Installing Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi

    rm -rf $TMP_DIR
    mkdir $TMP_DIR

    if [ "$REINSTALL" = true ] || [ ! -d "/usr/local/include/json" ]; then
        cd $TMP_DIR
        git clone https://github.com/open-source-parsers/jsoncpp.git
        cd jsoncpp
        mkdir build
        cd build
        cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=$CXX_PATH ..
        sudo make install
    fi

    if [ "$REINSTALL" = true ] || [ ! -d "/usr/local/include/drogon" ]; then
        cd $TMP_DIR
        git clone https://github.com/drogonframework/drogon.git
        cd drogon
        git submodule update --init
        mkdir build
        cd build
        cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=$CXX_PATH -DBUILD_EXAMPLES=OFF ..
        sudo make install
    fi

    if [ "$REINSTALL" = true ] || [ ! -d "/usr/local/include/poppler" ]; then
        cd $TMP_DIR
        wget https://poppler.freedesktop.org/poppler-24.09.0.tar.xz
        tar xjf poppler-24.09.0.tar.xz
        cd poppler-24.09.0
        mkdir build
        cd build
        
        cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=$CXX_PATH \
        -DENABLE_QT5=OFF -DENABLE_QT6=OFF -DENABLE_LIBCURL=OFF -DENABLE_GPGME=OFF ..

        sudo make install
    fi

    if [ "$REINSTALL" = true ] || [ ! -d "/usr/local/include/jwt-cpp" ]; then
        cd $TMP_DIR
        git clone https://github.com/Thalhammer/jwt-cpp.git
        sudo cp -rf jwt-cpp/include /usr/local/include
    fi

    rm -rf $TMP_DIR
}

# Determine OS and call appropriate installation function
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    install_linux
elif [[ "$OSTYPE" == "darwin"* ]]; then
    install_macos
else
    echo "Unsupported OS: $OSTYPE"
    exit 1
fi

echo "Library installation complete."
