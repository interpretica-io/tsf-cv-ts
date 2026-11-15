# Build environment
FROM debian:bookworm

ARG user
ARG uid
ARG gid
ARG docker_gid
ENV USER=${user}
ENV UID=${uid}
ENV GID=${gid}
ENV DOCKER_GID=${docker_gid}

RUN if [ "$DOCKER_GID" != "${GID}" ] ; then groupadd --gid ${DOCKER_GID} docker ; fi

RUN if [ ! -d /home/${USER} ] ; then mkdir -p /home/${USER} && chmod -R 777 /home/${USER} ; fi

# Install basic dependencies & useful tools. The OpenCV C++ API that
# ta_cv links is NOT the distribution libopencv-dev here: Debian's build
# ships without QUIRC (no QR decoding) and its 4.6 has no barcode module,
# so we build OpenCV from source below with both. g++ (build-essential)
# builds the one C++ unit, ta_cv_opencv.cpp; cmake + the libpng/jpeg/tiff
# -dev packages build OpenCV's codecs. The tesseract/leptonica packages
# and an English language pack make the optional OCR backend available.
# python3-opencv/numpy draw the fixtures (a separate process; the system
# cv2 is unrelated to the C++ OpenCV the agent links), python3-pil +
# python-barcode (pip) render the EAN-13 barcode fixture.
RUN apt-get update && \
    apt-get install -y \
        autoconf \
        automake \
        bison \
        build-essential \
        ccache \
        cmake \
        curl \
        docker.io \
        file \
        flex \
        gawk \
        git \
        libbsd-dev \
        libjpeg-dev \
        libpng-dev \
        libtiff-dev \
        zlib1g-dev \
        libtesseract-dev \
        libleptonica-dev \
        tesseract-ocr \
        tesseract-ocr-eng \
        python3-opencv \
        python3-numpy \
        python3-pil \
        python3-pip \
        libjansson-dev \
        libexpat1 \
        libexpat1-dev \
        libcurl4-openssl-dev \
        libglib2.0-dev \
        libnetconf2-dev \
        libncurses5 \
        libncurses5-dev \
        libpam0g \
        libpam0g-dev \
        libpcap0.8-dev \
        libpopt-dev \
        libreadline-dev \
        libssh-dev \
        libssh2-1-dev \
        libssl-dev \
        libusb-1.0-0 \
        libusb-1.0-0-dev \
        libxml2 \
        libxml2-dev \
        libxslt1-dev \
        libxslt1.1 \
        libpcre3-dev \
        libyaml-dev \
        m4 \
        meson \
        openssh-server \
        pkg-config \
        perl \
        python3 \
        rsync \
        ssh \
        sudo \
        tshark \
        tcl-dev \
        unzip \
        wget

# Build OpenCV from source with QUIRC (so QR codes actually decode) and
# the barcode module (part of objdetect from 4.8). Only the modules ta_cv
# uses are built - core, imgproc, imgcodecs, objdetect - which keeps the
# compile small; the heavy optional modules and bindings are off. It
# installs under /usr/local and generates opencv4.pc, which ta_cv's meson
# and build.sh --extra-deps=opencv4 both resolve via PKG_CONFIG_PATH
# below. QUIRC is vendored in opencv/3rdparty, so no extra download.
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
RUN cd /tmp && \
    git clone --depth 1 --branch 4.14.0 https://github.com/opencv/opencv.git && \
    cmake -S opencv -B opencv-build \
        -D CMAKE_BUILD_TYPE=Release \
        -D CMAKE_INSTALL_PREFIX=/usr/local \
        -D OPENCV_GENERATE_PKGCONFIG=ON \
        -D WITH_QUIRC=ON \
        -D BUILD_LIST=core,imgproc,imgcodecs,objdetect \
        -D BUILD_TESTS=OFF \
        -D BUILD_PERF_TESTS=OFF \
        -D BUILD_EXAMPLES=OFF \
        -D BUILD_opencv_apps=OFF \
        -D BUILD_opencv_python3=OFF \
        -D BUILD_opencv_python2=OFF \
        -D BUILD_JAVA=OFF \
        -D WITH_FFMPEG=OFF \
        -D WITH_GTK=OFF \
        -D WITH_V4L=OFF \
        -D WITH_GSTREAMER=OFF \
        -D WITH_PNG=ON \
        -D WITH_JPEG=ON \
        -D WITH_TIFF=ON && \
    cmake --build opencv-build -j"$(nproc)" && \
    cmake --install opencv-build && \
    echo /usr/local/lib > /etc/ld.so.conf.d/opencv.conf && ldconfig && \
    rm -rf /tmp/opencv /tmp/opencv-build

# OpenCV's own CI barcode reference images (opencv_extra testdata), the
# known-good control for the barcode test: these are real EAN-13 photos
# that OpenCV's regression tests assert cv::barcode decodes. If the agent
# decodes these but not our synthetic fixture, the fixture is at fault,
# not the library; if it fails even these, that is a genuine finding.
# Sparse, blobless checkout - just the three single-barcode JPEGs and
# their ground-truth CSV (bottle_1=6922255451427, bottle_2=6921168509256,
# book=9787115279460). This is a separate, cheap layer after the OpenCV
# compile above, so changing it does not rebuild OpenCV.
RUN cd /tmp && \
    git clone --filter=blob:none --no-checkout --depth 1 \
        https://github.com/opencv/opencv_extra.git && \
    cd opencv_extra && git sparse-checkout init --cone && \
    git sparse-checkout set testdata/cv/barcode && git checkout && \
    mkdir -p /opt/cv-barcode-ref && \
    cp testdata/cv/barcode/single/*.jpg /opt/cv-barcode-ref/ && \
    cp testdata/cv/barcode/single/result.csv /opt/cv-barcode-ref/ && \
    rm -rf /tmp/opencv_extra

# The fixture generator renders an EAN-13 barcode with python-barcode
# (pure Python; Pillow from python3-pil writes the PNG). OpenCV's barcode
# detector decodes EAN/UPC, so this gives the decode test a real 1-D code.
RUN pip3 install --no-cache-dir --break-system-packages 'python-barcode==0.15.1'

# The Test Agent application (ta_rpcprovider) is linked with the C
# compiler driver, so it does not pull in the C++ runtime that ta_cv's
# OpenCV unit needs. TE injects link libraries into that app only through
# pkg-config names given to build.sh --extra-deps, and libstdc++ ships no
# .pc, so provide a tiny one and name "stdcxx" in conf/builder.conf.
RUN mkdir -p /usr/lib/pkgconfig && \
    printf 'Name: stdcxx\nDescription: C++ runtime for the TE agent link\nVersion: 1\nLibs: -lstdc++\n' \
        > /usr/lib/pkgconfig/stdcxx.pc

RUN echo "${USER} ALL=NOPASSWD: ALL" >> /etc/sudoers

RUN if [ "$DOCKER_GID" != "${GID}" ] ; then groupadd --gid ${GID} ${USER} ; fi

RUN useradd --system \
            --gid ${GID} --gid ${DOCKER_GID} \
            --uid ${UID} \
            --shell /bin/bash \
            ${USER}
RUN usermod -a -G ${DOCKER_GID} ${USER}
RUN usermod -a -G docker ${USER}
