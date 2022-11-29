#! /bin/bash

shell_folder=$(cd "$(dirname "$0")" || exit; pwd)
export PREFIX="$shell_folder"/tools
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"
export BXSHARE="$PREFIX/tool/"

purple="\e[35m"
green="\e[32m"
red="\e[31m"
return='\e[0m'


if [[ $1 = '-h' ]] || [[ $1 = '--help' ]]; then
    echo "Init tools for downloading, compiling, installing debugger and corss-compile toolchain of JackOS, created by Jack Wang"
    echo 'Options:'
    echo '    -h, --help                Show this help message'
    echo '    -d, --download            Download toolchain'
    echo '    -c, --compile             Compile toolchain'
fi

if [[ $1 = "-d" ]] || [[ $1 = "--download" ]] || [[ ! -d "$shell_folder"/tools ]]; then
    if [[ ! -d "$shell_folder"/tools/src ]]; then
        read -p 'tools/src not exits, download? <y/n>: ' download
        if [[ $download = "y" ]]; then
            mkdir -p "$shell_folder"/tools/src
        fi
    fi
    if [[ $1 = "-d" ]] || [[ $1 = "--download" ]]; then
        echo 'Downloading debug tool'
        echo -e "$purple=> bochs-2.7$return" 
        if  [ -f "$shell_folder"/tools/src/bochs-2.7.tar.gz ]; then
            echo -e "${green}bochs download success$return"
        else
            if wget -t 5 -T 5 -c --quiet --show-progress -O "$shell_folder"/tools/src/bochs-2.7.tar.gz  https://sourceforge.net/projects/bochs/files/bochs/2.7/bochs-2.7.tar.gz ; then
                echo -e "${green}bochs download success$return"
            else
                echo -e "${red}bochs download fail, exiting...$return"
            fi
        fi
        echo 'Downloading cross-compiler...'
        echo -e "$purple=> gcc-10.4$return"
        if  wget -T 5 -c --quiet --show-progress -P "$shell_folder"/tools/src https://ftp.gnu.org/gnu/gcc/gcc-10.4.0/gcc-10.4.0.tar.gz; then
            echo -e "${green}gcc download success$return"
        else
            echo -e  "${red}gcc download fail, exiting...${return}" && exit
        fi
        echo -e "$purple=> binutils-2.7$return"
        if  wget -c -T 5 --quiet --show-progress -P "$shell_folder"/tools/src https://ftp.gnu.org/gnu/binutils/binutils-2.38.tar.gz;then
            echo -e "${green}binutils download success$return"
        else
            echo -e  "${green}binutils download fail, exiting...$return" && exit
        fi
    fi
fi

if [[ $1 = "-c" ]] || [[ $1 = "--compile" ]]; then
    log="$shell_folder"/tools/log
    mkdir -p "$log"
    echo -e "Target platform $green$TARGET${return}"
    echo -e "Cross-compile tools will be installed: ${green}$PREFIX$return"
    echo -e "Compile logs will be written to ${green}$log$return"
    echo "Compile may take 10 minutes, start in 3 seconds..."
    sleep 3s
    # bochs
    echo -e "$purple=> Compile bochs-2.7: no gdb$return"
    cd "$shell_folder"/tools/src || (echo "cd to tools/src fail, nothong changed, exiting..." && exit)
    echo "Extracting..."
    sleep 2s
    tar xzf "$shell_folder"/tools/src/bochs-2.7.tar.gz\
    && mkdir -p build-bochs\
    && cd "$shell_folder"/tools/src/build-bochs\
    && (../bochs-2.7/configure --prefix="$PREFIX" --enable-debugger --enable-iodebug --enable-x86-64-debugger --with-x --with-x11 2>&1 | tee "$log"/bochs-debugger-configure.log)\
    && (make -j "$(nproc)" 2>&1 | tee "$log"/bochs-debugger-make.log)\
    && (make install -j "$(nproc)" 2>&1 | tee "$log"/bochs-debugger-make-install.log)\
    && (../bochs-2.7/configure --prefix="$shell_folder"/tools/build-bochs-gdb --enable-gdb-stub --enable-iodebug --enable-x86-64-debugger --with-x --with-x11 2>&1 | tee "$log"/bochs-gdb-debugger-configure.log)\
    && (make -j "$(nproc)" 2>&1 | tee "$log"/bochs-gdb-make.log)\
    && (make install -j "$(nproc)" 2>&1 | tee "$log"/bochs-gdb-make-install.log)\
    && (mv "$shell_folder"/tools/build-bochs-gdb/bin/bochs "$shell_folder"/tools/bin/bochs-gdb)\
    && rm -r "$shell_folder"/tools/build-bochs-gdb
    echo -e "${green}bochs, bochs-gdb compile and install successfully$return, PS: ignore bochsdgb not found, it's no an error"

    # binutils
    echo -e "$purple=> Compile binutils.2.38$return"
    cd "$shell_folder"/tools/src || (echo "cd to tools/src fail, nothong changed, exiting..." && exit)
    echo "Extracting..."
    sleep 2s
    tar xzf "$shell_folder"/tools/src/binutils-2.38.tar.gz\
    && mkdir -p build-binutils\
    && cd "$shell_folder"/tools/src/build-binutils\
    && (../binutils-2.38/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror 2>&1 | tee "$log"/binutil-configure.log)\
    && (make -j "$(nproc)" 2>&1 | tee "$log"/binutil-make.log)\
    && (make install 2>&1 | tee "$log"/binutil-make-install.log)
    echo -e "${green}binutils compile and install successfully${return}"

    # gcc
    echo -e "$purple=> Compile gcc$return"
    cd "$shell_folder"/tools/src || (echo "cd to tools fail, nothong changed, exiting..." && exit)
    echo "search $TARGET-as..."
    which -- $TARGET-as || (echo "$TARGET-as is not in the PATH, aborting..."; exit)
    echo "Extracting..."
    sleep 2s
    tar xzf "$shell_folder"/tools/src/gcc-10.4.0.tar.gz\
    && mkdir -p build-gcc\
    && cd "$shell_folder"/tools/src/build-gcc || exit\
    && ../gcc-10.4.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-language=c,c++ --without-headers 2>&1 | tee "$log"/gcc-configure.log\
    && make -j "$(nproc)" all-gcc 2>&1 | tee "$log"/gcc-make-all-gcc.log\
    && make -j "$(nproc)" all-target-libgcc 2>&1 | tee "$log"/gcc-make-all-target-libgcc.log\
    && make install-gcc 2>&1 | tee "$log"/gcc-make-install-gcc.log\
    && make install-target-libgcc 2>&1 | tee "$log"/gcc-make-install-target-libgcc.log
    echo -e "${green}gcc compile and install successfully$return"

    cd "$shell_folder" || exit;
    echo "You can run bochs, bochs-gdb, $TARGET-gcc, $TARGET-ld, $TARGET-as now"
    echo -e "${green}"
    echo 'Run `source init.sh` first or manually add tools/bin into your PATH'
    echo -e "${return}"
fi
    
# fi
