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

# Install basic dependencies & useful tools. libopencv-dev gives the
# OpenCV C++ API (opencv2/..., libopencv_*) that ta_cv links; g++
# (build-essential) builds the one C++ unit, ta_cv_opencv.cpp. The
# tesseract/leptonica packages and an English language pack make the
# optional OCR backend available (python3-opencv/numpy are for the
# suite's fixture generator, not the module). python3-opencv pulls in
# the matching OpenCV runtime too.
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
        libbsd-dev \
        libopencv-dev \
        libtesseract-dev \
        libleptonica-dev \
        tesseract-ocr \
        tesseract-ocr-eng \
        python3-opencv \
        python3-numpy \
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
